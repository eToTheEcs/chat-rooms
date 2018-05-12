/*
 * this header contains the cryptographic utilities for implementing secure chats
 * in ChatRooms, using RSA algorithm
 * @author: Nicolas Benatti @01/2018
 */

#include <math.h>

typedef unsigned long long key_t;

key_t fastExp(key_t _base, key_t _exponent) {

  key_t res = 1;

  while(_exponent) {

    if(_exponent & 1)  res *= _base;

    _exponent >>= 1;
    _base *= _base;
  }

  return res;
}

// computes (_base^_exp) % _modVal
key_t modExp(key_t _base, key_t _exp, key_t modVal) {


  return fastExp(_base, _exponent) % _modVal;
}

// generate user private key
key_t generatePrivateKey() {


  
}


// generate public key (exchanger between the 2 users)
key_t generatePublic Key() {


}
