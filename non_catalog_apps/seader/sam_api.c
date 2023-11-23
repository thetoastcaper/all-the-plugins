
#include "sam_api.h"

#define TAG "SAMAPI"

#define APDU_HEADER_LEN 5
#define ASN1_PREFIX 6
#define ASN1_DEBUG true

#ifdef ASN1_DEBUG
char payloadDebug[384] = {0};
#endif

static char display[SEADER_UART_RX_BUF_SIZE * 2 + 1] = {0};
char asn1_log[SEADER_UART_RX_BUF_SIZE] = {0};
bool requestPacs = true;

void* calloc(size_t count, size_t size) {
    return malloc(count * size);
}

bool seader_send_apdu(
    SeaderUartBridge* seader_uart,
    uint8_t CLA,
    uint8_t INS,
    uint8_t P1,
    uint8_t P2,
    uint8_t* payload,
    uint8_t length) {
    if(APDU_HEADER_LEN + length > SEADER_UART_RX_BUF_SIZE) {
        FURI_LOG_E(TAG, "Cannot send message, too long: %d", APDU_HEADER_LEN + length);
        return false;
    }

    uint8_t* apdu = malloc(APDU_HEADER_LEN + length);
    apdu[0] = CLA;
    apdu[1] = INS;
    apdu[2] = P1;
    apdu[3] = P2;
    apdu[4] = length;
    memcpy(apdu + APDU_HEADER_LEN, payload, length);

    seader_ccid_XfrBlock(seader_uart, apdu, APDU_HEADER_LEN + length);
    free(apdu);
    return true;
}

static int seader_asn_to_string(const void* buffer, size_t size, void* app_key) {
    if(app_key) {
        char* str = (char*)app_key;
        size_t next = strlen(str);
        strncpy(str + next, buffer, size);
    } else {
        uint8_t next = strlen(asn1_log);
        strncpy(asn1_log + next, buffer, size);
    }
    return 0;
}

void seader_send_payload(
    SeaderUartBridge* seader_uart,
    Payload_t* payload,
    uint8_t to,
    uint8_t from,
    uint8_t replyTo) {
    uint8_t rBuffer[SEADER_UART_RX_BUF_SIZE] = {0};

    asn_enc_rval_t er = der_encode_to_buffer(
        &asn_DEF_Payload, payload, rBuffer + ASN1_PREFIX, sizeof(rBuffer) - ASN1_PREFIX);

#ifdef ASN1_DEBUG
    if(er.encoded > -1) {
        memset(payloadDebug, 0, sizeof(payloadDebug));
        (&asn_DEF_Payload)
            ->op->print_struct(&asn_DEF_Payload, payload, 1, seader_asn_to_string, payloadDebug);
        if(strlen(payloadDebug) > 0) {
            FURI_LOG_D(TAG, "Sending payload[%d %d %d]: %s", to, from, replyTo, payloadDebug);
        }
    }
#endif
    //0xa0, 0xda, 0x02, 0x63, 0x00, 0x00, 0x0a,
    //0x44, 0x0a, 0x44, 0x00, 0x00, 0x00, 0xa0, 0x02, 0x96, 0x00
    rBuffer[0] = to;
    rBuffer[1] = from;
    rBuffer[2] = replyTo;

    seader_send_apdu(seader_uart, 0xA0, 0xDA, 0x02, 0x63, rBuffer, 6 + er.encoded);
}

void seader_send_response(
    SeaderUartBridge* seader_uart,
    Response_t* response,
    uint8_t to,
    uint8_t from,
    uint8_t replyTo) {
    Payload_t* payload = 0;
    payload = calloc(1, sizeof *payload);
    assert(payload);

    payload->present = Payload_PR_response;
    payload->choice.response = *response;

    seader_send_payload(seader_uart, payload, to, from, replyTo);

    ASN_STRUCT_FREE(asn_DEF_Payload, payload);
}

void sendRequestPacs(SeaderUartBridge* seader_uart) {
    RequestPacs_t* requestPacs = 0;
    requestPacs = calloc(1, sizeof *requestPacs);
    assert(requestPacs);

    requestPacs->contentElementTag = ContentElementTag_implicitFormatPhysicalAccessBits;

    SamCommand_t* samCommand = 0;
    samCommand = calloc(1, sizeof *samCommand);
    assert(samCommand);

    samCommand->present = SamCommand_PR_requestPacs;
    samCommand->choice.requestPacs = *requestPacs;

    Payload_t* payload = 0;
    payload = calloc(1, sizeof *payload);
    assert(payload);

    payload->present = Payload_PR_samCommand;
    payload->choice.samCommand = *samCommand;

    seader_send_payload(seader_uart, payload, 0x44, 0x0a, 0x44);

    ASN_STRUCT_FREE(asn_DEF_RequestPacs, requestPacs);
    ASN_STRUCT_FREE(asn_DEF_SamCommand, samCommand);
    ASN_STRUCT_FREE(asn_DEF_Payload, payload);
}

void seader_worker_send_version(SeaderWorker* seader_worker) {
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SamCommand_t* samCommand = 0;
    samCommand = calloc(1, sizeof *samCommand);
    assert(samCommand);

    samCommand->present = SamCommand_PR_version;

    Payload_t* payload = 0;
    payload = calloc(1, sizeof *payload);
    assert(payload);

    payload->present = Payload_PR_samCommand;
    payload->choice.samCommand = *samCommand;

    seader_send_payload(seader_uart, payload, 0x44, 0x0a, 0x44);

    ASN_STRUCT_FREE(asn_DEF_SamCommand, samCommand);
    ASN_STRUCT_FREE(asn_DEF_Payload, payload);
}

void seader_send_card_detected(SeaderUartBridge* seader_uart, CardDetails_t* cardDetails) {
    CardDetected_t* cardDetected = 0;
    cardDetected = calloc(1, sizeof *cardDetected);
    assert(cardDetected);

    cardDetected->detectedCardDetails = *cardDetails;

    SamCommand_t* samCommand = 0;
    samCommand = calloc(1, sizeof *samCommand);
    assert(samCommand);

    samCommand->present = SamCommand_PR_cardDetected;
    samCommand->choice.cardDetected = *cardDetected;

    Payload_t* payload = 0;
    payload = calloc(1, sizeof *payload);
    assert(payload);

    payload->present = Payload_PR_samCommand;
    payload->choice.samCommand = *samCommand;

    seader_send_payload(seader_uart, payload, 0x44, 0x0a, 0x44);

    ASN_STRUCT_FREE(asn_DEF_CardDetected, cardDetected);
    ASN_STRUCT_FREE(asn_DEF_SamCommand, samCommand);
    ASN_STRUCT_FREE(asn_DEF_Payload, payload);
}

bool seader_unpack_pacs(SeaderCredential* seader_credential, uint8_t* buf, size_t size) {
    PAC_t* pac = 0;
    pac = calloc(1, sizeof *pac);
    assert(pac);
    bool rtn = false;

    asn_dec_rval_t rval = asn_decode(0, ATS_DER, &asn_DEF_PAC, (void**)&pac, buf, size);

    if(rval.code == RC_OK) {
        char pacDebug[384] = {0};
        (&asn_DEF_PAC)->op->print_struct(&asn_DEF_PAC, pac, 1, seader_asn_to_string, pacDebug);
        if(strlen(pacDebug) > 0) {
            FURI_LOG_D(TAG, "Received pac: %s", pacDebug);

            memset(display, 0, sizeof(display));
            if(seader_credential->sio[0] == 0x30) {
                for(uint8_t i = 0; i < sizeof(seader_credential->sio); i++) {
                    snprintf(
                        display + (i * 2), sizeof(display), "%02x", seader_credential->sio[i]);
                }
                FURI_LOG_D(TAG, "SIO %s", display);
            }
        }

        if(pac->size <= sizeof(seader_credential->credential)) {
            // TODO: make credential into a 12 byte array
            seader_credential->bit_length = pac->size * 8 - pac->bits_unused;
            memcpy(&seader_credential->credential, pac->buf, pac->size);
            seader_credential->credential = __builtin_bswap64(seader_credential->credential);
            seader_credential->credential = seader_credential->credential >>
                                            (64 - seader_credential->bit_length);
            rtn = true;
        } else {
            // PACS too big (probably bad data)
        }
    }

    ASN_STRUCT_FREE(asn_DEF_PAC, pac);
    return rtn;
}

//    800201298106683d052026b6820101
//300F800201298106683D052026B6820101
bool seader_parse_version(SeaderWorker* seader_worker, uint8_t* buf, size_t size) {
    SamVersion_t* version = 0;
    version = calloc(1, sizeof *version);
    assert(version);

    bool rtn = false;
    if(size > 30) {
        // Too large to handle now
        FURI_LOG_W(TAG, "Version of %d is to long to parse", size);
        return false;
    }
    // Add sequence prefix
    uint8_t seq[32] = {0x30};
    seq[1] = (uint8_t)size;
    memcpy(seq + 2, buf, size);

    asn_dec_rval_t rval =
        asn_decode(0, ATS_DER, &asn_DEF_SamVersion, (void**)&version, seq, size + 2);

    if(rval.code == RC_OK) {
        char versionDebug[128] = {0};
        (&asn_DEF_SamVersion)
            ->op->print_struct(
                &asn_DEF_SamVersion, version, 1, seader_asn_to_string, versionDebug);
        if(strlen(versionDebug) > 0) {
            // FURI_LOG_D(TAG, "Received version: %s", versionDebug);
        }
        if(version->version.size == 2) {
            memcpy(seader_worker->sam_version, version->version.buf, version->version.size);
        }

        rtn = true;
    }

    ASN_STRUCT_FREE(asn_DEF_SamVersion, version);
    return rtn;
}

bool seader_parse_sam_response(Seader* seader, SamResponse_t* samResponse) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    SeaderCredential* credential = seader->credential;

    if(samResponse->size == 0) {
        if(requestPacs) {
            FURI_LOG_D(TAG, "samResponse %d => requesting PACS", samResponse->size);
            sendRequestPacs(seader_uart);
            requestPacs = false;
        } else {
            FURI_LOG_D(TAG, "samResponse %d, no action", samResponse->size);
        }
    } else if(seader_parse_version(seader_worker, samResponse->buf, samResponse->size)) {
        // no-op
    } else if(seader_unpack_pacs(credential, samResponse->buf, samResponse->size)) {
        view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventWorkerExit);
    } else {
        memset(display, 0, sizeof(display));
        for(uint8_t i = 0; i < samResponse->size; i++) {
            snprintf(display + (i * 2), sizeof(display), "%02x", samResponse->buf[i]);
        }
        FURI_LOG_D(TAG, "Unknown samResponse %d: %s", samResponse->size, display);
    }

    return false;
}

bool seader_parse_response(Seader* seader, Response_t* response) {
    switch(response->present) {
    case Response_PR_samResponse:
        seader_parse_sam_response(seader, &response->choice.samResponse);
        break;
    default:
        break;
    };
    return false;
}

void seader_send_nfc_rx(SeaderUartBridge* seader_uart, uint8_t* buffer, size_t len) {
    OCTET_STRING_t rxData = {.buf = buffer, .size = len};
    uint8_t status[] = {0x00, 0x00};
    RfStatus_t rfStatus = {.buf = status, .size = 2};

    NFCRx_t* nfcRx = 0;
    nfcRx = calloc(1, sizeof *nfcRx);
    assert(nfcRx);

    nfcRx->rfStatus = rfStatus;
    nfcRx->data = &rxData;

    NFCResponse_t* nfcResponse = 0;
    nfcResponse = calloc(1, sizeof *nfcResponse);
    assert(nfcResponse);

    nfcResponse->present = NFCResponse_PR_nfcRx;
    nfcResponse->choice.nfcRx = *nfcRx;

    Response_t* response = 0;
    response = calloc(1, sizeof *response);
    assert(response);

    response->present = Response_PR_nfcResponse;
    response->choice.nfcResponse = *nfcResponse;

    seader_send_response(seader_uart, response, 0x14, 0x0a, 0x0);

    ASN_STRUCT_FREE(asn_DEF_NFCRx, nfcRx);
    ASN_STRUCT_FREE(asn_DEF_NFCResponse, nfcResponse);
    ASN_STRUCT_FREE(asn_DEF_Response, response);
}

static uint16_t seader_worker_picopass_update_ccitt(uint16_t crcSeed, uint8_t dataByte) {
    uint16_t crc = crcSeed;
    uint8_t dat = dataByte;

    dat ^= (uint8_t)(crc & 0xFFU);
    dat ^= (dat << 4);

    crc = (crc >> 8) ^ (((uint16_t)dat) << 8) ^ (((uint16_t)dat) << 3) ^ (((uint16_t)dat) >> 4);

    return crc;
}

static uint16_t seader_worker_picopass_calculate_ccitt(
    uint16_t preloadValue,
    const uint8_t* buf,
    uint16_t length) {
    uint16_t crc = preloadValue;
    uint16_t index;

    for(index = 0; index < length; index++) {
        crc = seader_worker_picopass_update_ccitt(crc, buf[index]);
    }

    return crc;
}

uint8_t read4Block6[] = {0x06, 0x06, 0x45, 0x56};
uint8_t read4Block9[] = {0x06, 0x09, 0xB2, 0xAE};
uint8_t read4Block10[] = {0x06, 0x0A, 0x29, 0x9C};
uint8_t read4Block13[] = {0x06, 0x0D, 0x96, 0xE8};
uint8_t updateBlock2[] = {0x87, 0x02}; // TODO

void seader_capture_sio(BitBuffer* tx_buffer, BitBuffer* rx_buffer, SeaderCredential* credential) {
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    size_t len = bit_buffer_get_size_bytes(tx_buffer);
    const uint8_t* rxBuffer = bit_buffer_get_data(rx_buffer);

    if(memcmp(buffer, read4Block6, len) == 0 && rxBuffer[0] == 0x30) {
        memcpy(credential->sio, rxBuffer, 32);
    } else if(memcmp(buffer, read4Block10, len) == 0 && rxBuffer[0] == 0x30) {
        memcpy(credential->sio, rxBuffer, 32);
    } else if(memcmp(buffer, read4Block9, len) == 0) {
        memcpy(credential->sio + 32, rxBuffer + 8, 24);
    } else if(memcmp(buffer, read4Block13, len) == 0) {
        memcpy(credential->sio + 32, rxBuffer + 8, 24);
    }
}

PicopassError seader_worker_fake_epurse_update(BitBuffer* tx_buffer, BitBuffer* rx_buffer) {
    const uint8_t* buffer = bit_buffer_get_data(tx_buffer);
    uint8_t fake_response[10];
    memset(fake_response, 0, sizeof(fake_response));
    memcpy(fake_response + 0, buffer + 6, 4);
    memcpy(fake_response + 4, buffer + 2, 4);

    uint16_t crc = seader_worker_picopass_calculate_ccitt(0xE012, fake_response, 8);
    memcpy(fake_response + 8, &crc, sizeof(uint16_t));

    bit_buffer_append_bytes(rx_buffer, fake_response, sizeof(fake_response));

    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < sizeof(fake_response); i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", fake_response[i]);
    }
    FURI_LOG_I(TAG, "Fake update E-Purse response: %s", display);

    return PicopassErrorNone;
}

void seader_iso15693_transmit(
    Seader* seader,
    PicopassPoller* picopass_poller,
    uint8_t* buffer,
    size_t len) {
    UNUSED(seader);
    UNUSED(buffer);
    UNUSED(len);

    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    PicopassError error = PicopassErrorNone;

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        if(memcmp(buffer, updateBlock2, sizeof(updateBlock2)) == 0) {
            error = seader_worker_fake_epurse_update(tx_buffer, rx_buffer);
        } else {
            error = picopass_poller_send_frame(
                picopass_poller, tx_buffer, rx_buffer, SEADER_POLLER_MAX_FWT);
        }
        if(error == PicopassErrorIncorrectCrc) {
            error = PicopassErrorNone;
        }

        if(error != PicopassErrorNone) {
            seader_worker->stage = SeaderPollerEventTypeFail;
            break;
        }

        seader_capture_sio(tx_buffer, rx_buffer, seader->credential);
        seader_send_nfc_rx(
            seader_uart,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

/* Assumes this is called in the context of the NFC API callback */
void seader_iso14443a_transmit(
    Seader* seader,
    Iso14443_4aPoller* iso14443_4a_poller,
    uint8_t* buffer,
    size_t len,
    uint16_t timeout,
    uint8_t format[3]) {
    UNUSED(timeout);
    UNUSED(format);

    furi_assert(seader);
    furi_assert(buffer);
    furi_assert(iso14443_4a_poller);
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;

    BitBuffer* tx_buffer = bit_buffer_alloc(len);
    BitBuffer* rx_buffer = bit_buffer_alloc(SEADER_POLLER_MAX_BUFFER_SIZE);

    do {
        bit_buffer_append_bytes(tx_buffer, buffer, len);

        Iso14443_4aError error =
            iso14443_4a_poller_send_block(iso14443_4a_poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4aErrorNone) {
            FURI_LOG_W(TAG, "iso14443_4a_poller_send_block error %d", error);
            seader_worker->stage = SeaderPollerEventTypeFail;
            break;
        }

        seader_send_nfc_rx(
            seader_uart,
            (uint8_t*)bit_buffer_get_data(rx_buffer),
            bit_buffer_get_size_bytes(rx_buffer));

    } while(false);
    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);
}

void seader_parse_nfc_command_transmit(
    Seader* seader,
    NFCSend_t* nfcSend,
    SeaderPollerContainer* spc) {
    long timeOut = nfcSend->timeOut;
    Protocol_t protocol = nfcSend->protocol;
    FrameProtocol_t frameProtocol = protocol.buf[1];

#ifdef ASN1_DEBUG
    memset(display, 0, sizeof(display));
    for(uint8_t i = 0; i < nfcSend->data.size; i++) {
        snprintf(display + (i * 2), sizeof(display), "%02x", nfcSend->data.buf[i]);
    }

    FURI_LOG_D(
        TAG,
        "Transmit (%ld timeout) %d bytes [%s] via %lx",
        timeOut,
        nfcSend->data.size,
        display,
        frameProtocol);
#endif

    if(frameProtocol == FrameProtocol_iclass) {
        seader_iso15693_transmit(
            seader, spc->picopass_poller, nfcSend->data.buf, nfcSend->data.size);
    } else if(frameProtocol == FrameProtocol_nfc) {
        seader_iso14443a_transmit(
            seader,
            spc->iso14443_4a_poller,
            nfcSend->data.buf,
            nfcSend->data.size,
            (uint16_t)timeOut,
            nfcSend->format->buf);
    } else {
        FURI_LOG_W(TAG, "unknown frame protocol %lx", frameProtocol);
    }
}

void seader_parse_nfc_off(SeaderUartBridge* seader_uart) {
    FURI_LOG_D(TAG, "Set Field Off");

    NFCResponse_t* nfcResponse = 0;
    nfcResponse = calloc(1, sizeof *nfcResponse);
    assert(nfcResponse);

    nfcResponse->present = NFCResponse_PR_nfcAck;

    Response_t* response = 0;
    response = calloc(1, sizeof *response);
    assert(response);

    response->present = Response_PR_nfcResponse;
    response->choice.nfcResponse = *nfcResponse;

    seader_send_response(seader_uart, response, 0x44, 0x0a, 0);

    ASN_STRUCT_FREE(asn_DEF_Response, response);
    ASN_STRUCT_FREE(asn_DEF_NFCResponse, nfcResponse);
}

void seader_parse_nfc_command(Seader* seader, NFCCommand_t* nfcCommand, SeaderPollerContainer* spc) {
    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    switch(nfcCommand->present) {
    case NFCCommand_PR_nfcSend:
        seader_parse_nfc_command_transmit(seader, &nfcCommand->choice.nfcSend, spc);
        break;
    case NFCCommand_PR_nfcOff:
        seader_parse_nfc_off(seader_uart);
        seader->worker->stage = SeaderPollerEventTypeComplete;
        break;
    default:
        FURI_LOG_W(TAG, "unparsed NFCCommand");
        break;
    };
}

bool seader_worker_state_machine(
    Seader* seader,
    Payload_t* payload,
    bool online,
    SeaderPollerContainer* spc) {
    bool processed = false;

    switch(payload->present) {
    case Payload_PR_response:
        seader_parse_response(seader, &payload->choice.response);
        processed = true;
        break;
    case Payload_PR_nfcCommand:
        if(online) {
            seader_parse_nfc_command(seader, &payload->choice.nfcCommand, spc);
            processed = true;
        }
        break;
    case Payload_PR_errorResponse:
        FURI_LOG_W(TAG, "Error Response");
        processed = true;
        break;
    default:
        FURI_LOG_W(TAG, "unhandled payload");
        break;
    };

    return processed;
}

bool seader_process_success_response_i(
    Seader* seader,
    uint8_t* apdu,
    size_t len,
    bool online,
    SeaderPollerContainer* spc) {
    Payload_t* payload = 0;
    payload = calloc(1, sizeof *payload);
    assert(payload);
    bool processed = false;

    asn_dec_rval_t rval =
        asn_decode(0, ATS_DER, &asn_DEF_Payload, (void**)&payload, apdu + 6, len - 6);
    if(rval.code == RC_OK) {
        processed = seader_worker_state_machine(seader, payload, online, spc);

#ifdef ASN1_DEBUG
        if(processed) {
            memset(payloadDebug, 0, sizeof(payloadDebug));
            (&asn_DEF_Payload)
                ->op->print_struct(
                    &asn_DEF_Payload, payload, 1, seader_asn_to_string, payloadDebug);
            if(strlen(payloadDebug) > 0) {
                FURI_LOG_D(TAG, "Received payload: %s", payloadDebug);
            }
        }
#endif
    } else {
        FURI_LOG_D(TAG, "Failed to decode APDU payload");
    }

    ASN_STRUCT_FREE(asn_DEF_Payload, payload);
    return processed;
}

NfcCommand seader_worker_card_detect(
    Seader* seader,
    uint8_t sak,
    uint8_t* atqa,
    const uint8_t* uid,
    uint8_t uid_len,
    uint8_t* ats,
    uint8_t ats_len) {
    UNUSED(ats);
    UNUSED(ats_len);

    // We're telling the SAM we've seen a new card, so reset out requestPacs check
    requestPacs = true;

    SeaderWorker* seader_worker = seader->worker;
    SeaderUartBridge* seader_uart = seader_worker->uart;
    CardDetails_t* cardDetails = 0;
    cardDetails = calloc(1, sizeof *cardDetails);
    assert(cardDetails);

    OCTET_STRING_fromBuf(&cardDetails->csn, (const char*)uid, uid_len);

    if(sak != 0 && atqa != NULL) {
        uint8_t protocol_bytes[] = {0x00, FrameProtocol_nfc};
        OCTET_STRING_fromBuf(
            &cardDetails->protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));

        OCTET_STRING_t sak_string = {.buf = &sak, .size = 1};
        cardDetails->sak = &sak_string;

        OCTET_STRING_t atqa_string = {.buf = atqa, .size = 2};
        cardDetails->atqa = &atqa_string;

    } else {
        uint8_t protocol_bytes[] = {0x00, FrameProtocol_iclass};
        OCTET_STRING_fromBuf(
            &cardDetails->protocol, (const char*)protocol_bytes, sizeof(protocol_bytes));
    }

    seader_send_card_detected(seader_uart, cardDetails);

    ASN_STRUCT_FREE(asn_DEF_CardDetails, cardDetails);

    return NfcCommandContinue;
}