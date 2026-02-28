#include "logging.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <memory>

// tee_streambuf writes to two streambufs
class tee_streambuf : public std::streambuf {
public:
    tee_streambuf(std::streambuf* sb1, std::streambuf* sb2) : sb1_(sb1), sb2_(sb2) {}
protected:
    int overflow(int c) override {
        if (c == EOF) return !EOF;
        const char ch = static_cast<char>(c);
        if (sb1_ && sb1_->sputc(ch) == EOF) return EOF;
        if (sb2_ && sb2_->sputc(ch) == EOF) return EOF;
        return c;
    }

    int sync() override {
        int r1 = 0, r2 = 0;
        if (sb1_) r1 = sb1_->pubsync();
        if (sb2_) r2 = sb2_->pubsync();
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }

private:
    std::streambuf* sb1_;
    std::streambuf* sb2_;
};

static std::ofstream g_logfile;
static std::unique_ptr<tee_streambuf> g_coutbuf;
static std::unique_ptr<tee_streambuf> g_cerrbuf;
static std::streambuf* g_old_cout = nullptr;
static std::streambuf* g_old_cerr = nullptr;

void init_logging(const std::string& path) {
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        g_logfile.open(path, std::ios::out | std::ios::app);
        if (!g_logfile.is_open()) {
            std::cerr << "logging: failed to open log file: " << path << std::endl;
            return;
        }
        g_old_cout = std::cout.rdbuf();
        g_old_cerr = std::cerr.rdbuf();
        g_coutbuf = std::make_unique<tee_streambuf>(g_old_cout, g_logfile.rdbuf());
        g_cerrbuf = std::make_unique<tee_streambuf>(g_old_cerr, g_logfile.rdbuf());
        std::cout.rdbuf(g_coutbuf.get());
        std::cerr.rdbuf(g_cerrbuf.get());
        std::cout << "-- Log started --" << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "logging init exception: " << ex.what() << std::endl;
    }
}

void shutdown_logging() {
    if (g_old_cout) std::cout.rdbuf(g_old_cout);
    if (g_old_cerr) std::cerr.rdbuf(g_old_cerr);
    if (g_logfile.is_open()) {
        std::cout << "-- Log ended --" << std::endl;
        g_logfile.close();
    }
}
