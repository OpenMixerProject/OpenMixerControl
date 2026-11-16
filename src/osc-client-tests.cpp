#include "osc-client.h"
#include "../lib_ext/small-osc/small-osc.h"
#include "../lib_ext/doctest/doctest/doctest.h"

namespace OMC
{
    TEST_CASE("OSC client sends string and numeric parameter types on the wire")
    {
        int listener = socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE(listener >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(10032);
        REQUIRE(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        timeval timeout{1, 0};
        setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        Helper helper{};
        Config config("X32CORE", &helper, true);
        config.GetParameter(MP_ID::CHANNEL_NAME)->DefOSC("ch_name");
        CLI::App app;
        app.add_option("--client")->default_val("127.0.0.1");
        State state{};
        X32BaseParameter base(&app, &config, &state, &helper);
        OscClient client(&base);
        REQUIRE(client.Init() == 0);
        std::string name(350, 'x');
        client.UdpSendToServerSet(MP_ID::CHANNEL_NAME, name.c_str(), 0, 2);
        char data[1024];
        int size = recv(listener, data, sizeof(data), 0);
        REQUIRE(size > 0);
        tosc_message message;
        REQUIRE(tosc_parseMessage(&message, data, size) == 0);
        CHECK(std::string(tosc_getAddress(&message)) == "/set/ch_name/3");
        CHECK(std::string(tosc_getFormat(&message)) == "s");
        CHECK(std::string(tosc_getNextString(&message)) == name);
        client.UdpSendToServerSet(MP_ID::CHANNEL_GAIN, "ignored", 12.5f, 0);
        size = recv(listener, data, sizeof(data), 0);
        REQUIRE(size > 0);
        REQUIRE(tosc_parseMessage(&message, data, size) == 0);
        CHECK(std::string(tosc_getFormat(&message)) == "f");
        CHECK(tosc_getNextFloat(&message) == 12.5f);
        close(client.UdpHandle);
        close(listener);
    }
}
