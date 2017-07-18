//
//  JCHttpClient.cpp
//  curlhttp
//
//  Created by dawenhing on 13/03/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#include "JCHttpClient.hpp"

#include <curl/curl.h>
#include <string>

#define CURL_DEBUG 0
#define CURL_DEBUG_SHOW_DATA 0

#if CURL_DEBUG
static int OnDebug(CURL *curl, curl_infotype itype, char *pData, size_t size, void *context) {
    if(itype == CURLINFO_TEXT) {
        //printf("[TEXT]%s\n", pData);
    }
    else if(itype == CURLINFO_HEADER_IN) {
        printf("[HEADER_IN]%s\n", pData);
    }
    else if(itype == CURLINFO_HEADER_OUT) {
        printf("[HEADER_OUT]%s\n", pData);
    }
#if CURL_DEBUG_SHOW_DATA
    else if(itype == CURLINFO_DATA_IN) {
        printf("[DATA_IN]%s\n", pData);
    }
    else if(itype == CURLINFO_DATA_OUT) {
        printf("[DATA_OUT]%s\n", pData);
    }
#endif
    return 0;
}
#endif

static size_t OnWriteString(void *buffer, size_t size, size_t nmemb, void *context)
{
    std::string *str = dynamic_cast<std::string*>((std::string *)context);
    if( NULL == str || NULL == buffer ) {
        return -1;
    }
    
    char *pData = (char *)buffer;
    str->append(pData, size * nmemb);
    return nmemb;
}

const char *JCHttpResponse::strerror(int error) {
    return curl_easy_strerror(static_cast<CURLcode>(error)); 
}

int JCHttpClient::Request(const JCHttpRequest &request, JCHttpResponse &response) {
    
    CURLcode res;
    CURL* curl = curl_easy_init();
    if(NULL == curl) {
        return CURLE_FAILED_INIT;
    }
#if CURL_DEBUG
    {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, OnDebug);
    }
#endif
    
    curl_easy_setopt(curl, CURLOPT_URL, request.URL.c_str());
    if (request.method == JCHttpPostMethod) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    }
    if (request.body.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, nullptr);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response.responseText);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (request.customCAPath.length() > 0) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, true);
        curl_easy_setopt(curl, CURLOPT_CAINFO, request.customCAPath.c_str());
    }
    if (request.connectTimeout > 0) {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connectTimeout);
    }
    if (request.timeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout);
    }
    
    struct curl_slist *headers = nullptr;
    for (auto s: request.headers) {
        headers = curl_slist_append(headers, s.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    res = curl_easy_perform(curl);
    
    long statusCode;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode) == CURLE_OK) {
        response.statusCode = statusCode;
    }
    
    curl_easy_cleanup(curl);    
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    return res;    
}

int JCHttpClient::Post(const std::string &strUrl, const std::string &strParams, std::string &strResponse) {
    JCHttpPostRequest request;
    request.URL = strUrl;
    request.body = strParams;
    JCHttpResponse response;
    int res = JCHttpClient::Request(request, response);
    strResponse = response.responseText;
    return res;
}

std::string JCHttpClient::EscapeParam(const std::string &param) {
    char *enc = curl_escape(param.c_str(), (int)param.length());
    if (enc) {
        std::string encodedParam(enc);
        curl_free(enc);
        return encodedParam;
    }
    return param;
}

int JCHttpClient::Get(const std::string &strUrl, std::string &strResponse)
{
    JCHttpGetRequest request;
    request.URL = strUrl;
    JCHttpResponse response;
    int res = JCHttpClient::Request(request, response);
    strResponse = response.responseText;
    return res;
}

struct JCDownloadContextInternal {
    bool cancel;
    bool paused;    // pause trigger flag
    bool unpaused;  // unpaused trigger flag
    FILE *fp;
    CURL *curl;
    JCDownloadContext::JCDownloadProgressType onProgress;
    JCDownloadContextInternal() {
        fp = nullptr;
        curl = nullptr;
        cancel = false;
        paused = false;
        unpaused = false;
        onProgress = nullptr;
    }
};

void JCDownloadContext::cancel() {
    static_cast<JCDownloadContextInternal*>(internal)->cancel = true;
}

void JCDownloadContext::pause() {
    static_cast<JCDownloadContextInternal*>(internal)->paused = true;
}

void JCDownloadContext::resume() {
    static_cast<JCDownloadContextInternal*>(internal)->unpaused = true;
}

JCDownloadContext::JCDownloadContext() {
    internal = new JCDownloadContextInternal;
}

JCDownloadContext::~JCDownloadContext() {
    delete static_cast<JCDownloadContextInternal*>(internal);
}

static size_t OnWriteFile(void *ptr, size_t size, size_t nmemb, void *context) {
    JCDownloadContextInternal *internalContext = static_cast<JCDownloadContextInternal *>(context);
    size_t written = fwrite(ptr, size, nmemb, internalContext->fp);
    return written;
}

static int OnWriteFileProgress(void *context, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    JCDownloadContextInternal *internalContext = static_cast<JCDownloadContextInternal *>(context);
    if (internalContext->cancel) {
        return -1;
    }
    else if (internalContext->paused) {    
        curl_easy_pause(internalContext->curl, CURLPAUSE_ALL);
        // apply only once
        internalContext->paused = false;
    }
    else if (internalContext->unpaused) {
        curl_easy_pause(internalContext->curl, CURLPAUSE_CONT);
        // apply only once
        internalContext->unpaused = false;
    }
    if (internalContext->onProgress) {
        return internalContext->onProgress(dltotal, dlnow);
    }
    return 0;
}

int JCHttpClient::Download(const JCHttpDownloadRequest &request, JCDownloadContext &context, JCHttpResponse &response) {
    CURL *curl = curl_easy_init();
    if(NULL == curl) {
        return CURLE_FAILED_INIT;
    }
#if CURL_DEBUG
    {
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, OnDebug);
    }
#endif
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
    // If server failed (status code > 400) we don't download that erro HTML page.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    if (request.connectTimeout > 0) {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, request.connectTimeout);
    }
    
    if (request.timeout > 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, request.timeout);
    }

    FILE *fp = nullptr;
    if (request.resumeSize > 0) {
        fp = fopen(request.savePath.c_str(), "ab");
        if (fp) {
            long resumeAt = 0;
            if (fseek(fp, request.resumeSize, SEEK_SET) == 0) {
                resumeAt = request.resumeSize;
            }
            if (resumeAt > 0) {
                char range[64];
                snprintf(range, sizeof(range)-1, "%ld-", resumeAt);
                if (curl_easy_setopt(curl, CURLOPT_RANGE, range) != CURLE_OK) {
                    fclose(fp);
                    fp = fopen(request.savePath.c_str(), "wb");
                }
            }
        }
    }
    else {
        fp = fopen(request.savePath.c_str(), "wb");        
    }
    
    if (!fp) {
        curl_easy_cleanup(curl);
        return CURLE_WRITE_ERROR;
    }
    
    CURLcode res = curl_easy_setopt(curl, CURLOPT_URL, request.URL.c_str());
    if (res != CURLE_OK) {
        fclose(fp);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnWriteFile);
    if (res != CURLE_OK) {
        fclose(fp);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    JCDownloadContextInternal *internalContext = static_cast<JCDownloadContextInternal *>(context.internal);
    internalContext->fp = fp;
    internalContext->curl = curl;    

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, internalContext);
    if (res != CURLE_OK) {
        fclose(fp);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    if (context.onProgress) {
        internalContext->onProgress = context.onProgress;
    }
    
    int ret = (int)curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    ret |= (int)curl_easy_setopt(curl, CURLOPT_XFERINFODATA, internalContext);
    ret |= (int)curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, OnWriteFileProgress);
    
    if (ret != CURLE_OK) {
        fclose(fp);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    struct curl_slist *headers = nullptr;
    for (auto s: request.headers) {
        headers = curl_slist_append(headers, s.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }
    
    res = curl_easy_perform(curl);
    
    long statusCode;
    if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode) == CURLE_OK) {
        response.statusCode = statusCode;
    }

    fclose(fp);
    curl_easy_cleanup(curl);  
    if (headers) {
        curl_slist_free_all(headers);
    }
    return res;  
}
