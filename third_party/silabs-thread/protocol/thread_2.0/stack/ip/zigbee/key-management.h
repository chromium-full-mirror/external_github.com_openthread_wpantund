/*
 * File: key-management.h
 * Description: key management for ZigBee-IP
 *
 * Copyright 2012 by Ember Corporation. All rights reserved.                *80*
 */

#define LEGACY_KEY_SEQUENCE_NUMBER 1

// In the MAC AUX frame Thread uses 1-128 to encode the low seven bits
// of the actual key sequence number.  This gives us a window of
//  [activeKeySequence - 64, activeKeySequence + 63]
// For each neighbor and child we are synchronized with we store
//    theirActiveKeySequence - ourActiveKeySequence
// as an int8_u whose value is in [-64, 63].

void emClearNetworkKeys(void);
void emRestoreKeys(void);
void emResetKeys(void);
void emFormSecurityInit(void);
void emJoinSecurityInit(void);

const uint8_t *emGetNetworkMasterKey(uint8_t *storage);
void emPickRandomKey(uint8_t sequence);
void emSetNetworkMasterKey(uint8_t const *key, uint32_t sequence);

uint8_t *emDeriveMleAndMacKeys(uint32_t sequence, uint8_t *keyBuffer);
void emSetNetworkKeySequence(uint32_t sequence);
bool emIsActiveKeySequence(uint32_t keySequence);
bool emIsPriorKeySequence(uint32_t keySequence);
const uint8_t *emGetActiveMacKeyAndSequence(uint32_t *sequenceNumberLoc);
void emNoteMacKeyUse(uint32_t keySequence);

// When using an 8-bit MAC sequence number field Thread sends the
// low seven bits of the 32-bit key sequence number, plus one.  The
// one is added because 802.15.4 does not allow the sequence number
// field to be zero.  The over-the-air values used are 1-128.  The
// 129-255 values are currently reserved.

#define emShrinkSequenceNumber(sequence) ((sequence & 0x7F) + 1)
bool emIsShortActiveKeySequence(uint8_t shortKeySequence);
uint32_t emExpandSequenceNumber(uint8_t shortKeySequence);

bool emSetKeySequenceDelta(uint32_t keySequence,
                           int8_t *sequenceDeltaLoc);

/*
bool emSequenceNumberIsNewer(uint32_t sequenceNumber);
bool emDeriveKey(uint32_t sequence,
                 uint8_t *derivedKey,
                 uint8_t *legacyKey);
*/
