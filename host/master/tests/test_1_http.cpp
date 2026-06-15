#include <iostream>
#include <fcgio.h>

using namespace std;

int main(void) {
    // 备份 C++ 标准的 streambuf
    streambuf * cin_streambuf  = cin.rdbuf();
    streambuf * cout_streambuf = cout.rdbuf();
    streambuf * cerr_streambuf = cerr.rdbuf();

    FCGX_Request request;
    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);

    // 阻塞等待 Nginx 转发过来的请求
    while (FCGX_Accept_r(&request) == 0) {
        // 将请求的输入输出流绑定到 fcgi_streambuf
        fcgi_streambuf cin_fcgi_streambuf(request.in);
        fcgi_streambuf cout_fcgi_streambuf(request.out);
        fcgi_streambuf cerr_fcgi_streambuf(request.err);

        cin.rdbuf(&cin_fcgi_streambuf);
        cout.rdbuf(&cout_fcgi_streambuf);
        cerr.rdbuf(&cerr_fcgi_streambuf);

        // 1. 输出 HTTP 响应头（必须以两个 \r\n 结束）
        cout << "Content-type: application/json\r\n"
             << "\r\n";

        // 2. 输出实际的 Payload 数据
        cout << "{\n"
             << "  \"status\": \"success\",\n"
             << "  \"message\": \"Hello from C++ FastCGI backend\",\n"
             << "  \"device_type\": \"gateway_node\"\n"
             << "}\n";
    }

    // 恢复标准流（通常只有在进程被终止时才会执行到这里）
    cin.rdbuf(cin_streambuf);
    cout.rdbuf(cout_streambuf);
    cerr.rdbuf(cerr_streambuf);

    return 0;
}
