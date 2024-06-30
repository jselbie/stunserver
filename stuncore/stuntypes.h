/*
   Copyright 2011 John Selbie

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/



#ifndef STUNTYPES_H
#define STUNTYPES_H

// RFC 5389: STUN RFC
// RFC 3489: OLD STUN RFC. Obsolete, but newer specs reference it
// RFC 5769 - test vectors for STUN
// RFC 5780 - Nat behavior discovery using STUN



const uint16_t DEFAULT_STUN_PORT = 3478;
const uint16_t DEFAULT_STUN_TLS_PORT = 5349;


const uint16_t STUN_ATTRIBUTE_MAPPEDADDRESS   = 0x0001;
const uint16_t STUN_ATTRIBUTE_RESPONSEADDRESS = 0x0002;
const uint16_t STUN_ATTRIBUTE_CHANGEREQUEST   = 0x0003;
const uint16_t STUN_ATTRIBUTE_SOURCEADDRESS   = 0x0004;
const uint16_t STUN_ATTRIBUTE_CHANGEDADDRESS  = 0x0005; // this is the legacy "other address" from rfc 3489, superceded by STUN_ATTRIBUTE_OTHERADDRESS below

const uint16_t STUN_ATTRIBUTE_USERNAME = 0x0006;

const uint16_t STUN_ATTRIBUTE_LEGACY_PASSWORD = 0x0007; // old rfc


const uint16_t STUN_ATTRIBUTE_MESSAGEINTEGRITY = 0x0008;
const uint16_t STUN_ATTRIBUTE_ERRORCODE = 0x0009;
const uint16_t STUN_ATTRIBUTE_UNKNOWNATTRIBUTES = 0x000A;

const uint16_t STUN_ATTRIBUTE_REFLECTEDFROM = 0x000B; // old rfc


const uint16_t STUN_ATTRIBUTE_REALM = 0x0014;
const uint16_t STUN_ATTRIBUTE_NONCE = 0x0015;
const uint16_t STUN_ATTRIBUTE_XORMAPPEDADDRESS = 0x0020;

const uint16_t STUN_ATTRIBUTE_PADDING = 0x0026;
const uint16_t STUN_ATTRIBUTE_RESPONSE_PORT = 0x0027;



// This attribute is sent by the server to legacy clients
// 0x8020 is is not defined in any RFC, but is the value that Vovida server uses
const uint16_t STUN_ATTRIBUTE_XORMAPPEDADDRESS_OPTIONAL = 0x8020;

const uint16_t STUN_ATTRIBUTE_SOFTWARE = 0x8022;
const uint16_t STUN_ATTRIBUTE_ALTERNATESERVER = 0x8023;

const uint16_t STUN_ATTRIBUTE_FINGERPRINT = 0x8028;

const uint16_t STUN_ATTRIBUTE_RESPONSE_ORIGIN = 0x802b;
const uint16_t STUN_ATTRIBUTE_OTHER_ADDRESS = 0x802c;




const uint16_t STUN_TRANSACTION_ID_LENGTH = 16;

const uint8_t STUN_ATTRIBUTE_FIELD_IPV4 = 1;
const uint8_t STUN_ATTRIBUTE_FIELD_IPV6 = 2;

const uint16_t STUN_IPV4_LENGTH = 4;
const uint16_t STUN_IPV6_LENGTH = 16;


const uint16_t STUN_ERROR_TRYALTERNATE = 300;
const uint16_t STUN_ERROR_BADREQUEST = 400;
const uint16_t STUN_ERROR_UNAUTHORIZED = 401;
const uint16_t STUN_ERROR_UNKNOWNATTRIB = 420;
const uint16_t STUN_ERROR_STALENONCE = 438;
const uint16_t STUN_ERROR_SERVERERROR = 500;



enum StunMessageClass
{
    StunMsgClassRequest=0x00,
    StunMsgClassIndication=0x01,
    StunMsgClassSuccessResponse=0x02,
    StunMsgClassFailureResponse=0x03,
    StunMsgClassInvalidMessageClass = 0xff
};

enum StunMessageType
{
    StunMsgTypeBinding = 0x0001,
    StunMsgTypeInvalid = 0xffff
};

struct StunAttribute
{
    uint16_t attributeType;
    uint16_t size;
    uint16_t offset;
};

struct StunMappedAddressAttribute_IPV4
{
    uint8_t zero;
    uint8_t family;
    uint16_t xport;
    uint32_t xip;
};

struct StunMappedAddressAttribute_IPV6
{
    uint8_t zero;
    uint8_t family;
    uint16_t xport;
    uint8_t xip[16];
};

struct StunChangeRequestAttribute
{
    bool fChangeIP;
    bool fChangePort;
};


const uint16_t STUN_ATTRIBUTE_MAPPEDADDRESS_SIZE_IPV4 = 8;
const uint16_t STUN_ATTRIBUTE_MAPPEDADDRESS_SIZE_IPV6 = 20;
const uint16_t STUN_ATTRIBUTE_RESPONSE_PORT_SIZE = 2;
const uint16_t STUN_ATTRIBUTE_CHANGEREQUEST_SIZE = 4;



#define STUN_IS_REQUEST(msg_type)       (((msg_type) & 0x0110) == 0x0000)
#define STUN_IS_INDICATION(msg_type)    (((msg_type) & 0x0110) == 0x0010)
#define STUN_IS_SUCCESS_RESP(msg_type)  (((msg_type) & 0x0110) == 0x0100)
#define STUN_IS_ERR_RESP(msg_type)      (((msg_type) & 0x0110) == 0x0110)



struct StunTransactionId
{
    uint8_t id[STUN_TRANSACTION_ID_LENGTH]; // first four bytes is usually the magic cookie field
};

inline bool operator==(const StunTransactionId &id1, const StunTransactionId &id2)
{
    return (0 == memcmp(id1.id, id2.id, STUN_TRANSACTION_ID_LENGTH));
}




const uint32_t STUN_COOKIE = 0x2112A442;
const uint8_t STUN_COOKIE_B1 = 0x21;
const uint8_t STUN_COOKIE_B2 = 0x12;
const uint8_t STUN_COOKIE_B3 = 0xA4;
const uint8_t STUN_COOKIE_B4 = 0x42;


const uint32_t STUN_FINGERPRINT_XOR = 0x5354554e;

const uint16_t STUN_XOR_PORT_COOKIE = 0x2112;

const uint32_t STUN_HEADER_SIZE = 20;

const uint32_t MAX_STUN_MESSAGE_SIZE = 800; // some reasonable length
const uint32_t MAX_STUN_ATTRIBUTE_SIZE = 780; // more than reasonable

enum NatBehavior
{
    UnknownBehavior,
    DirectMapping,                  // IP address and port are the same between client and server view (NO NAT)
    EndpointIndependentMapping,     // same mapping regardless of IP:port original packet sent to (the kind of NAT we like)
    AddressDependentMapping,        // mapping changes for local socket based on remote IP address only, but remote port can change (partially symmetric, not great)
    AddressAndPortDependentMapping  // different port mapping if the ip address or port change (symmetric NAT, difficult to predict port mappings)
};

enum NatFiltering
{
    UnknownFiltering,
    DirectConnectionFiltering,
    EndpointIndependentFiltering,    // shouldn't be common unless connection is already direct (can receive on mapped address from anywhere regardless of where the original send went)
    AddressDependentFiltering,       // IP-restricted NAT
    AddressAndPortDependentFiltering // port-restricted NAT
};


#endif
