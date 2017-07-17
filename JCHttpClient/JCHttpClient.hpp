//
//  JCHttpClient.hpp
//  curlhttp
//
//  Created by dawenhing on 13/03/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#ifndef JCHttpClient_hpp
#define JCHttpClient_hpp

#include <string>
#include <map>
#include <vector>
#include <functional>

/**
 * Context for download
 */
class JCDownloadContext {
private:
    void *internal;
    
public:
    JCDownloadContext();
    ~JCDownloadContext();
    JCDownloadContext(const JCDownloadContext &other) = delete;
    JCDownloadContext operator = (const JCDownloadContext &other) = delete;
    friend class JCHttpClient;
    
    void cancel();
    void pause();
    void resume();

    using JCDownloadProgressType = std::function<int (long total, long now)>;
    
    // Download progress callback function
    JCDownloadProgressType onProgress;
};

/**
 * HTTP request method
 */
enum JCHttpMethod : int {
    JCHttpInvalidMethod = 0,
    JCHttpPostMethod = 1,
    JCHttpGetMethod  = 2,
};

/**
 * HTTP request parameters
 */
struct JCHttpRequest {
    JCHttpRequest() { connectTimeout = 5; timeout = 10; method = JCHttpInvalidMethod; }
    std::string URL;
    JCHttpMethod method;
    std::string body;
    std::vector<std::string> headers;
    
    // timeout for establish connection, 0 means not set this option
    long connectTimeout;
    
    // timeout for the total operation, 0 means not set this option
    long timeout;
    
    // the custom CA path for HTTPS with custom certification
    std::string customCAPath;
};

/**
 * HTTP Get reuqest
 */
struct JCHttpGetRequest: JCHttpRequest {
    JCHttpGetRequest() { method = JCHttpGetMethod; }
};

/**
 * HTTP Post reuqest
 */
struct JCHttpPostRequest: JCHttpRequest {
    JCHttpPostRequest() { method = JCHttpPostMethod; }
};

/**
 * HTTP Download reuqest
 */
struct JCHttpDownloadRequest: JCHttpRequest {
    // Download will need much time, so timeout isn't suitable for such case
    JCHttpDownloadRequest() { resumeSize = 0; timeout = 0;}
    
    std::string savePath;
    
    // resume from which size, normally it is the size of partial downloaded file.
    long resumeSize;
};

struct JCHttpResponse {
    JCHttpResponse() { statusCode = 200; textEncoding = "utf8"; }
    long statusCode;
    std::string responseText;
    std::string textEncoding;
    std::vector<std::string> headers;
    
    static const char *strerror(int error);
};

class JCHttpClient {
public:
    /**
     * @brief HTTP Request
     * @param request parameters for request
     * @param response [out] response from server
     * @return libcurl function result
     */
    static int Request(const JCHttpRequest &request, JCHttpResponse &response);
    /**
     * @brief HTTP Download
     * @param request parameters for request
     * @param context [in/out], the download context which control the download progress.
     * @param response [out] response from server
     * @return libcurl function result
     */
    static int Download(const JCHttpDownloadRequest &request, JCDownloadContext &context, JCHttpResponse &response);
    
    /**
     * @brief HTTP Post request
     * @param strUrl server address
     * @param strParams parameters for request
     * @param strResponse [out] response from server
     * @return libcurl function result
     */
    static int Post(const std::string &strUrl, const std::string &strParams, std::string &strResponse);
    
    /**
     * @brief HTTP Get request
     * @param strUrl server address with parameters
     * @param strResponse [out] response from server
     * @return libcurl function result
     */
    static int Get(const std::string &strUrl, std::string &strResponse);
    
    static std::string EscapeParam(const std::string &request);
};

#endif /* JCHttpClient_hpp */
