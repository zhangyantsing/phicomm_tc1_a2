/*
 * This file is part of libemqtt.
 *
 * libemqtt is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libemqtt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libemqtt.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *
 * Created by Filipe Varela on 09/10/16.
 * Copyright 2009 Caixa Mágica Software. All rights reserved.
 *
 * Fork developed by Vicente Ruiz Rodríguez
 * Copyright 2012 Vicente Ruiz Rodríguez <vruiz2.0@gmail.com>. All rights reserved.
 *
 */

#include <string.h>
#include "mqttlib.h"

#define MQTT_DUP_FLAG     1<<3
#define MQTT_QOS0_FLAG    0<<1
#define MQTT_QOS1_FLAG    1<<1
#define MQTT_QOS2_FLAG    2<<1

#define MQTT_RETAIN_FLAG  1

#define MQTT_CLEAN_SESSION  1<<1
#define MQTT_WILL_FLAG      1<<2
#define MQTT_WILL_QoS       1<<3
#define MQTT_WILL_RETAIN    1<<5
#define MQTT_USERNAME_FLAG  1<<7
#define MQTT_PASSWORD_FLAG  1<<6


uint8_t mqtt_num_rem_len_bytes(const uint8_t* buf) {
    uint8_t num_bytes = 1;
    
    //printf("mqtt_num_rem_len_bytes\n");
    
    if ((buf[1] & 0x80) == 0x80) {
        num_bytes++;
        if ((buf[2] & 0x80) == 0x80) {
            num_bytes ++;
            if ((buf[3] & 0x80) == 0x80) {
                num_bytes ++;
            }
        }
    }
    return num_bytes;
}

uint16_t mqtt_parse_rem_len(const uint8_t* buf) {
    uint16_t multiplier = 1;
    uint16_t value = 0;
    uint8_t digit;
    
    //printf("mqtt_parse_rem_len\n");
    
    buf++;  // skip "flags" byte in fixed header

    do {
        digit = *buf;
        value += (digit & 127) * multiplier;
        multiplier *= 128;
        buf++;
    } while ((digit & 128) != 0);

    return value;
}

uint16_t mqtt_parse_msg_id(const uint8_t* buf) {
    uint8_t type = MQTTParseMessageType(buf);
    uint8_t qos = MQTTParseMessageQos(buf);
    uint16_t id = 0;
    
    //printf("mqtt_parse_msg_id\n");
    
    if(type >= MQTT_MSG_PUBLISH && type <= MQTT_MSG_UNSUBACK) {
        if(type == MQTT_MSG_PUBLISH) {
            if(qos != 0) {
                // fixed header length + Topic (UTF encoded)
                // = 1 for "flags" byte + rlb for length bytes + topic size
                uint8_t rlb = mqtt_num_rem_len_bytes(buf);
                uint8_t offset = *(buf+1+rlb)<<8;   // topic UTF MSB
                offset |= *(buf+1+rlb+1);           // topic UTF LSB
                offset += (1+rlb+2);                    // fixed header + topic size
                id = *(buf+offset)<<8;              // id MSB
                id |= *(buf+offset+1);              // id LSB
            }
        } else {
            // fixed header length
            // 1 for "flags" byte + rlb for length bytes
            uint8_t rlb = mqtt_num_rem_len_bytes(buf);
            id = *(buf+1+rlb)<<8;   // id MSB
            id |= *(buf+1+rlb+1);   // id LSB
        }
    }

	return id;
}

uint16_t mqtt_parse_pub_topic(const uint8_t* buf, uint8_t* topic) {
    const uint8_t* ptr;
    uint16_t topic_len = mqtt_parse_pub_topic_ptr(buf, &ptr);
    
    //printf("mqtt_parse_pub_topic\n");
    
    if(topic_len != 0 && ptr != NULL) {
        memcpy(topic, ptr, topic_len);
    }
    
    return topic_len;
}

uint16_t mqtt_parse_pub_topic_ptr(const uint8_t* buf, const uint8_t **topic_ptr) {
    uint16_t len = 0;
    
    //printf("mqtt_parse_pub_topic_ptr\n");

    if(MQTTParseMessageType(buf) == MQTT_MSG_PUBLISH) {
        // fixed header length = 1 for "flags" byte + rlb for length bytes
        uint8_t rlb = mqtt_num_rem_len_bytes(buf);
        len = *(buf+1+rlb)<<8;  // MSB of topic UTF
        len |= *(buf+1+rlb+1);  // LSB of topic UTF
        // start of topic = add 1 for "flags", rlb for remaining length, 2 for UTF
        *topic_ptr = (buf + (1+rlb+2));
    } else {
        *topic_ptr = NULL;
    }
    return len;
}

uint16_t mqtt_parse_publish_msg(const uint8_t* buf, uint8_t* msg) {
    const uint8_t* ptr;
    
    //printf("mqtt_parse_publish_msg\n");
    
    uint16_t msg_len = mqtt_parse_pub_msg_ptr(buf, &ptr);
    
    if(msg_len != 0 && ptr != NULL) {
        memcpy(msg, ptr, msg_len);
    }
    
    return msg_len;
}

uint16_t mqtt_parse_pub_msg_ptr(const uint8_t* buf, const uint8_t **msg_ptr) {
    uint16_t len = 0;
    
    //printf("mqtt_parse_pub_msg_ptr\n");
    
    if(MQTTParseMessageType(buf) == MQTT_MSG_PUBLISH) {
        // message starts at
        // fixed header length + Topic (UTF encoded) + msg id (if QoS>0)
        uint8_t rlb = mqtt_num_rem_len_bytes(buf);
        uint8_t offset = (*(buf+1+rlb))<<8; // topic UTF MSB
        offset |= *(buf+1+rlb+1);           // topic UTF LSB
        offset += (1+rlb+2);                // fixed header + topic size

        if(MQTTParseMessageQos(buf)) {
            offset += 2;                    // add two bytes of msg id
        }

        *msg_ptr = (buf + offset);
                
        // offset is now pointing to start of message
        // length of the message is remaining length - variable header
        // variable header is offset - fixed header
        // fixed header is 1 + rlb
        // so, lom = remlen - (offset - (1+rlb))
        len = mqtt_parse_rem_len(buf) - (offset-(rlb+1));
    } else {
        *msg_ptr = NULL;
    }
    return len;
}

void mqtt_init(mqtt_broker_handle_t* broker, const char* clientid) {
    // Connection options
    broker->alive = 300; // 300 seconds = 5 minutes
    broker->seq = 1; // Sequency for message indetifiers
    // Client options
    memset(broker->clientid, 0, sizeof(broker->clientid));
    memset(broker->username, 0, sizeof(broker->username));
    memset(broker->password, 0, sizeof(broker->password));
    if(clientid) {
        strncpy(broker->clientid, clientid, sizeof(broker->clientid));
    } else {
        strcpy(broker->clientid, "emqtt");
    }
    // Will topic
    broker->clean_session = 1;
}

void mqtt_init_auth(mqtt_broker_handle_t* broker, const char* username, const char* password) {
    if(username && username[0] != '\0')
        strncpy(broker->username, username, sizeof(broker->username)-1);
    if(password && password[0] != '\0')
        strncpy(broker->password, password, sizeof(broker->password)-1);
}

void mqtt_set_alive(mqtt_broker_handle_t* broker, uint16_t alive) {
    broker->alive = alive;
}

int mqtt_connect(mqtt_broker_handle_t* broker)
{
    uint8_t flags = 0x00;

    uint16_t clientidlen  = strlen(broker->clientid);
	uint16_t willtopiclen = strlen(broker->will_topic);
	uint16_t willmsglen = strlen(broker->will_msg);
    uint16_t usernamelen  = strlen(broker->username);
    uint16_t passwordlen  = strlen(broker->password);
    uint16_t payload_len  = clientidlen + 2;

    // Preparing the flags
    if(usernamelen)
	{
        payload_len += usernamelen + 2;
        flags |= MQTT_USERNAME_FLAG;
    }
    if(passwordlen) 
	{
        payload_len += passwordlen + 2;
        flags |= MQTT_PASSWORD_FLAG;
    }
    if(broker->clean_session)
	{
        flags |= MQTT_CLEAN_SESSION;
    }
	if(broker->will_flag) 
	{
		payload_len += willtopiclen+2;
		payload_len += willmsglen  +2;
        flags |= MQTT_WILL_FLAG;
    }
	if(broker->will_qos) 
	{
        flags |= MQTT_WILL_QoS;
    }

#ifdef MQTT_VERSION_USE_3_1 //VERSION:3.1
    // Variable header
    uint8_t var_header[] = {
        0x00,0x06,0x4d,0x51,0x49,0x73,0x64,0x70, // Protocol name: MQIsdp
        0x03, // Protocol version
        flags, // Connect flags
        broker->alive>>8, broker->alive&0xFF, // Keep alive
    };
#else//VERSION:3.1.1
     // Variable header
    uint8_t var_header[] = {
        0x00,0x04,0x4d,0x51,0x54,0x54, // Protocol name: MQTT
        0x04, // Protocol version
        flags, // Connect flags
        broker->alive>>8, broker->alive&0xFF, // Keep alive
    };
#endif

    // Fixed header
    uint8_t fixedHeaderSize = 2;    // Default size = one byte Message Type + one byte Remaining Length
    uint8_t remainLen = sizeof(var_header)+payload_len;
    if (remainLen > 127) {
        fixedHeaderSize++;          // add an additional byte for Remaining Length
    }
	
    uint8_t fixed_header[10];
    
    // Message Type
    fixed_header[0] = MQTT_MSG_CONNECT;

    // Remaining Length
    if (remainLen <= 127) 
	{
        fixed_header[1] = remainLen;
    } 
	else
	{
        // first byte is remainder (mod) of 128, then set the MSB to indicate more bytes
        fixed_header[1] = remainLen % 128;
        fixed_header[1] = fixed_header[1] | 0x80;
        // second byte is number of 128s
        fixed_header[2] = remainLen / 128;
    }

    uint16_t offset = 0;
    int packet_len = fixedHeaderSize+sizeof(var_header)+payload_len;
    uint8_t packet[256];
    memset(packet, 0, 256);
    memcpy(packet, fixed_header, fixedHeaderSize);
    offset += fixedHeaderSize;
    memcpy(packet+offset, var_header, sizeof(var_header));
    offset += sizeof(var_header);
    // Client ID - UTF encoded
    packet[offset++] = clientidlen>>8;
    packet[offset++] = clientidlen&0xFF;
    memcpy(packet+offset, broker->clientid, clientidlen);
    offset += clientidlen;

	 // willtopic - UTF encoded
    if(willtopiclen)
	{
	    packet[offset++] = willtopiclen>>8;
	    packet[offset++] = willtopiclen&0xFF;
	    memcpy(packet+offset, broker->will_topic, willtopiclen);
	    offset += willtopiclen;
	}

	 // willmsg - UTF encoded
	if(willmsglen)
	{
	    packet[offset++] = willmsglen>>8;
	    packet[offset++] = willmsglen&0xFF;
	    memcpy(packet+offset, broker->will_msg, willmsglen);
	    offset += willmsglen;
	}
    if(usernamelen) {
        // Username - UTF encoded
        packet[offset++] = usernamelen>>8;
        packet[offset++] = usernamelen&0xFF;
        memcpy(packet+offset, broker->username, usernamelen);
        offset += usernamelen;
    }

    if(passwordlen) {
        // Password - UTF encoded
        packet[offset++] = passwordlen>>8;
        packet[offset++] = passwordlen&0xFF;
        memcpy(packet+offset, broker->password, passwordlen);
        offset += passwordlen;
    }

    // Send the packet
    if(broker->send(broker->socket_info, packet, packet_len) < packet_len) {
        return -1;
    }

    return 1;
}

int mqtt_disconnect(mqtt_broker_handle_t* broker) {
    uint8_t packet[] = {
        MQTT_MSG_DISCONNECT, // Message Type, DUP flag, QoS level, Retain
        0x00 // Remaining length
    };

    // Send the packet
    if(broker->send(broker->socket_info, packet, sizeof(packet)) < sizeof(packet)) {
        return -1;
    }

    return 1;
}

int mqtt_ping(mqtt_broker_handle_t* broker) {
    uint8_t packet[] = {
        MQTT_MSG_PINGREQ, // Message Type, DUP flag, QoS level, Retain
        0x00 // Remaining length
    };

    // Send the packet
    if(broker->send(broker->socket_info, packet, sizeof(packet)) < sizeof(packet)) {
        return -1;
    }

    return 1;
}

int upload_len = 0;
int mqtt_publish(mqtt_broker_handle_t* broker, const char* topic, const char* msg, uint8_t retain) {
    return mqtt_publish_with_qos(broker, topic, msg, retain, 0, NULL);
}

int mqtt_publish_with_qos(mqtt_broker_handle_t* broker, const char* topic, const char* msg, uint8_t retain, uint8_t qos, uint16_t* message_id) {
    uint16_t topiclen = strlen(topic);
	  uint16_t msglen = upload_len;
    //uint16_t msglen = strlen(msg);
extern void *hfmem_malloc(int size);
	char * packet_p;
packet_p = (char *)hfmem_malloc(topiclen+msglen+20);
	if(packet_p == NULL)
		return -1;
	memset(packet_p, 0 , topiclen+msglen+20);
	int packet_len = 0;
	
    uint8_t qos_flag = MQTT_QOS0_FLAG;
    uint8_t qos_size = 0; // No QoS included
    if(qos == 1) {
        qos_size = 2; // 2 bytes for QoS
        qos_flag = MQTT_QOS1_FLAG;
    }
    else if(qos == 2) {
        qos_size = 2; // 2 bytes for QoS
        qos_flag = MQTT_QOS2_FLAG;
    }

    // Variable header
    uint8_t var_header[2]; // Topic size (2 bytes), utf-encoded topic
    uint8_t var_header_qos[2];
	
    memset(var_header, 0, sizeof(var_header));
    var_header[0] = topiclen>>8;
    var_header[1] = topiclen&0xFF;
   // memcpy(var_header+2, topic, topiclen);
    if(qos_size) {
        var_header_qos[0] = broker->seq>>8;
        var_header_qos[1] = broker->seq&0xFF;
        if(message_id) { // Returning message id
            *message_id = broker->seq;
        }
        broker->seq++;
    }

    // Fixed header
    // the remaining length is one byte for messages up to 127 bytes, then two bytes after that
    // actually, it can be up to 4 bytes but I'm making the assumption the embedded device will only
    // need up to two bytes of length (handles up to 16,383 (almost 16k) sized message)
    uint8_t fixedHeaderSize = 2;    // Default size = one byte Message Type + one byte Remaining Length
    uint16_t remainLen = sizeof(var_header)+topiclen+qos_size+msglen;
    if (remainLen > 127) {
        fixedHeaderSize++;          // add an additional byte for Remaining Length
    }
    uint8_t fixed_header[3];
    
   // Message Type, DUP flag, QoS level, Retain
   fixed_header[0] = MQTT_MSG_PUBLISH | qos_flag;
    if(retain) {
        fixed_header[0] |= MQTT_RETAIN_FLAG;
   }
   // Remaining Length
   if (remainLen <= 127) {
       fixed_header[1] = remainLen;
   } else {
       // first byte is remainder (mod) of 128, then set the MSB to indicate more bytes
       fixed_header[1] = remainLen % 128;
       fixed_header[1] = fixed_header[1] | 0x80;
       // second byte is number of 128s
       fixed_header[2] = remainLen / 128;
   }

    //uint8_t packet[sizeof(fixed_header)+sizeof(var_header)+msglen];
    //memset(packet, 0, sizeof(packet));
    memcpy(packet_p, fixed_header, fixedHeaderSize); packet_len+= fixedHeaderSize;
    memcpy(packet_p+packet_len, var_header, 2); packet_len+= 2;
    memcpy(packet_p+packet_len, topic, topiclen); packet_len+= topiclen;
    if(qos_size)
         memcpy(packet_p+packet_len, var_header_qos, qos_size); packet_len+= qos_size;
    memcpy(packet_p+packet_len, msg, msglen); packet_len+= (msglen);
extern void hfmem_free(void *pv);
    // Send the packet
    if(broker->send(broker->socket_info, packet_p, packet_len) < packet_len) {
		hfmem_free(packet_p);
        return -1;
    }
hfmem_free(packet_p);
    return 1;
}

int mqtt_pubrel(mqtt_broker_handle_t* broker, uint16_t message_id) {
    uint8_t packet[] = {
        MQTT_MSG_PUBREL | MQTT_QOS1_FLAG, // Message Type, DUP flag, QoS level, Retain
        0x02, // Remaining length
        message_id>>8,
        message_id&0xFF
    };

    // Send the packet
    if(broker->send(broker->socket_info, packet, sizeof(packet)) < sizeof(packet)) {
        return -1;
    }

    return 1;
}

int mqtt_subscribe(mqtt_broker_handle_t* broker, const char* topic, uint16_t* message_id) {
    uint16_t topiclen = strlen(topic);

    // Variable header
    uint8_t var_header[2]; // Message ID
    var_header[0] = broker->seq>>8;
    var_header[1] = broker->seq&0xFF;
    if(message_id) { // Returning message id
        *message_id = broker->seq;
    }
    broker->seq++;

    // utf topic
    uint8_t utf_topic[2]; // Topic size (2 bytes), utf-encoded topic, QoS byte
    memset(utf_topic, 0, sizeof(utf_topic));
    utf_topic[0] = topiclen>>8;
    utf_topic[1] = topiclen&0xFF;

    // Fixed header
    uint8_t fixed_header[] = {
        MQTT_MSG_SUBSCRIBE | MQTT_QOS1_FLAG, // Message Type, DUP flag, QoS level, Retain
        sizeof(var_header)+sizeof(utf_topic)
    };
    fixed_header[1] = sizeof(var_header)+sizeof(utf_topic)+topiclen+1/*Qos*/;//msg_len

    uint8_t packet[128];
    int packet_len=0;
    memset(packet, 0, 128);
    memcpy(packet+packet_len, fixed_header, sizeof(fixed_header));packet_len+=sizeof(fixed_header);
    memcpy(packet+packet_len, var_header, sizeof(var_header));packet_len+=sizeof(var_header);
    memcpy(packet+packet_len, utf_topic, sizeof(utf_topic));packet_len+=sizeof(utf_topic);
    memcpy(packet+packet_len, topic, topiclen);packet_len+=topiclen;
    packet[packet_len++] = MQTT_QOS1_FLAG;//Qos

    // Send the packet
    if(broker->send(broker->socket_info, packet, packet_len) < packet_len) {
        return -1;
    }

    return 1;
}

int mqtt_unsubscribe(mqtt_broker_handle_t* broker, const char* topic, uint16_t* message_id) {
    uint16_t topiclen = strlen(topic);

    // Variable header
    uint8_t var_header[2]; // Message ID
    var_header[0] = broker->seq>>8;
    var_header[1] = broker->seq&0xFF;
    if(message_id) { // Returning message id
        *message_id = broker->seq;
    }
    broker->seq++;

    // utf topic
    uint8_t utf_topic[2]; // Topic size (2 bytes), utf-encoded topic
    memset(utf_topic, 0, sizeof(utf_topic));
    utf_topic[0] = topiclen>>8;
    utf_topic[1] = topiclen&0xFF;
   // memcpy(utf_topic+2, topic, topiclen);

    // Fixed header
    uint8_t fixed_header[] = {
        MQTT_MSG_UNSUBSCRIBE | MQTT_QOS1_FLAG, // Message Type, DUP flag, QoS level, Retain
        sizeof(var_header)+sizeof(utf_topic)
    };

    uint8_t packet[128];
    int packet_len=0;
    memset(packet, 0, 128);
    memcpy(packet+packet_len, fixed_header, sizeof(fixed_header));packet_len+=sizeof(fixed_header);
    memcpy(packet+packet_len, var_header, sizeof(var_header));packet_len+=sizeof(var_header);
    memcpy(packet+packet_len, utf_topic, sizeof(utf_topic));packet_len+=sizeof(utf_topic);
    memcpy(packet+packet_len, topic, topiclen);packet_len+=(topiclen+1);

    // Send the packet
    if(broker->send(broker->socket_info, packet, packet_len) < packet_len) {
        return -1;
    }

    return 1;
}

int mqtt_puback(mqtt_broker_handle_t* broker, uint16_t message_id) {
    
    uint8_t packet[4]={0x40, 0x02};
    packet[2] = (message_id&0xFF00)>>8;
    packet[3] = message_id&0xFF;
    if(broker->send(broker->socket_info, packet, 4) < 4) {
        return -1;
    }
	
    return 1;
}


