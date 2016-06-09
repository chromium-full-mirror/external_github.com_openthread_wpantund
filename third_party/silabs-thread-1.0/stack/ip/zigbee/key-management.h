/*
 * File: key-management.h
 * Description: key management for ZigBee-IP
 *
 * Copyright 2012 by Ember Corporation. All rights reserved.                *80*
 */

void emClearNetworkKeys(void);
void emRestoreKeys(void);
void emResetKeys(void);
void emNetworkKeyUpdate(uint32_t sequence, uint8_t const *key, uint8_t authCounter);
void emGetMasterKeyAuthCounter(uint32_t sessionId,
                               bool newNodeJoining,
                               uint8_t *keyRequest,
                               uint8_t *sequence,
                               uint8_t *masterKey,
                               uint8_t *authCounter);
void emPaaAddressNotification(uint32_t sessionId, uint8_t const *newAddress);
void emNewPaaMasterKey(uint8_t sequence, uint8_t const *newMasterKey);
void emActivateNewPaaMasterKey(void);
void emMarkPaaBuffers(void);
void emPrintPaaTable(uint8_t port);
uint8_t *emGetMasterKey(uint32_t sequenceNumber);
void emFormSecurityInit(void);
void emJoinSecurityInit(void);
bool emIsActiveKeySequence(uint8_t keySequence);
bool emIsPriorKeySequence(uint8_t keySequence);
uint32_t emExpandSequenceNumber(uint8_t littleSequence);
#define emShrinkSequenceNumber(sequence) ((sequence & 0x7F) + 1)

typedef enum {
 SWITCHED_TO_NEW_KEY,
 FRAME_COUNTER_VALID,
 FRAME_COUNTER_OTHER_FRAGMENT,
 FRAME_COUNTER_NOT_VALID_HIGH,
 FRAME_COUNTER_NOT_VALID
} FrameCounterStatus;

FrameCounterStatus emIsFrameCounterValid(uint32_t newFrameCounter,
                                         uint32_t keySequence,
                                         uint32_t nextFrameCounter,
                                         bool usingOldKey,
                                         bool isLegacy);

extern bool emThreadEncryptionEnabled;

void emSetSequenceNumber(uint32_t amount);
void emIncrementSequenceNumber(void);
bool emSequenceNumberIsNewer(uint32_t sequenceNumber);
bool emDeriveKey(uint32_t sequence,
                    const uint8_t *masterKey,
                    uint8_t *derivedKey,
                    uint8_t *legacyKey);

#define LEGACY_KEY_SEQUENCE_NUMBER 1
