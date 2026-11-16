#include "osc-server.h"
#include "../lib_ext/doctest/doctest/doctest.h"
#include <filesystem>
#include <fstream>

namespace OMC
{
    TEST_CASE("OSC scene commands report persistence outcomes and unsupported recorder")
    {
        struct Directory
        {
            std::filesystem::path old = std::filesystem::current_path();
            char path[64] = "/tmp/omc-scene-test-XXXXXX";
            Directory() { REQUIRE(mkdtemp(path) != nullptr); std::filesystem::current_path(path); }
            ~Directory() { std::filesystem::current_path(old); std::filesystem::remove_all(path); }
        } directory;
        Helper helper{};
        Config config("X32CORE", &helper, false);
        CLI::App app;
        State state{};
        state.bodyless = true;
        X32BaseParameter base(&app, &config, &state, &helper);
        OscServer server(&base);
        REQUIRE(server.Init() == 0);
        int listener = socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE(listener >= 0);
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        target.sin_port = htons(10033);
        REQUIRE(bind(listener, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0);
        timeval timeout{1, 0};
        setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        target.sin_port = htons(10032);
        auto command = [&](const char* path, bool success)
        {
            char data[512];
            int size = tosc_writeMessage(data, sizeof(data), path, "");
            sendto(listener, data, size, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
            server.UdpHandleCommunication();
            size = recv(listener, data, sizeof(data), 0);
            REQUIRE(size > 0);
            tosc_message message;
            REQUIRE(tosc_parseMessage(&message, data, size) == 0);
            CHECK(std::string(tosc_getAddress(&message)) == std::string("/result") + path);
            std::string json(tosc_getNextString(&message));
            CHECK(json.find(success ? "\"success\":true" : "\"success\":false") != std::string::npos);
            return json;
        };
        config.Set(MP_ID::CHANNEL_GAIN, 12.0f, 0);
        config.Set(MP_ID::CHANNEL_NAME, String("Lead vocal"), 0);
        command("/scene/save/1", true);
        config.Set(MP_ID::CHANNEL_GAIN, 4.0f, 0);
        config.Set(MP_ID::CHANNEL_NAME, String("Changed"), 0);
        command("/scene/load/1", true);
        CHECK(config.GetFloat(MP_ID::CHANNEL_GAIN, 0) == 12.0f);
        CHECK(config.GetString(MP_ID::CHANNEL_NAME, 0) == String("Lead vocal"));
        command("/scene/load/2", false);
        std::ofstream("scene2.json") << "invalid json";
        command("/scene/load/2", false);
        std::filesystem::create_directory("scene3.json");
        command("/scene/save/3", false);
        command("/scene/load/-1", false);
        command("/scene/save/10000", false);
        CHECK(command("/recorder/stop/1", false).find("unsupported_no_xlive_card") != std::string::npos);
        close(listener);
        close(server.UdpHandle);
    }
}
