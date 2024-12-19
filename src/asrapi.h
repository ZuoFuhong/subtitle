#pragma once

#include <string>

enum ASRCode {
    ERROR_OK = 0,
    ERROR_PARA = 1,
};

using HANDLE = void*;

ASRCode ASR_create_session(HANDLE& session);

ASRCode ASR_begin_session(HANDLE session);

ASRCode ASR_end_session(HANDLE session);

ASRCode ASR_push_buffer(HANDLE session, const float* pdata, unsigned int nlen);

ASRCode ASR_get_vad_state(HANDLE session, int* state);

ASRCode ASR_get_result(HANDLE session, std::string& res);
