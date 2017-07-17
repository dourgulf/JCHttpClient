//
//  main.cpp
//  JCHttpClient
//
//  Created by dawenhing on 17/07/2017.
//  Copyright © 2017 dawenhing. All rights reserved.
//

#include <iostream>
#include <thread>

#include "JCHttpClient.hpp"

using namespace std;

int main(int argc, const char * argv[]) {
    volatile bool stopCommonLoop = false;
    
    JCDownloadContext context;
    std::thread download([&context, &stopCommonLoop]{
        context.onProgress = [](long total, long now) {
            // 回调可能是在curl的"主线程"触发的，为了不堵塞下载，把进度处理抛到异步线程比较好
            cout << now << "/" << total << endl;
            return 0;
        };
        JCHttpDownloadRequest request;
        request.URL = "http://www.baidu.com";
        request.savePath = "download.result";        
        request.headers.push_back("UserAgent: curl");
        JCHttpResponse response;
        int ret = JCHttpClient::Download(request, context, response);
        cout << ret << ":" << JCHttpResponse::strerror(ret) << endl;
        cout << response.statusCode << endl;
        cout << response.responseText << endl;
        stopCommonLoop = true;
    });
    download.detach();
    std::thread commondLoop([&context, &stopCommonLoop]{
        std::this_thread::sleep_for(std::chrono::seconds(2));
        context.pause();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        context.resume();
        while (!stopCommonLoop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    commondLoop.join();
}
