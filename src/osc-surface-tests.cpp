#include "osc-server.h"
#include "../lib_ext/small-osc/small-osc.h"
#include "../lib_ext/doctest/doctest/doctest.h"

namespace OMC
{
    TEST_CASE("OSC surface observation preserves bindings and supports unsubscribe")
    {
        Helper helper{};
        Config config("X32CORE", &helper, false);
        CLI::App app;
        State state{};
        X32BaseParameter base(&app, &config, &state, &helper);
        OscServer server(&base);
        REQUIRE(server.Init() == 0);
        int listener = socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE(listener >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(10033);
        REQUIRE(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
        timeval timeout{0, 100000};
        setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        address.sin_port = htons(10032);
        char data[256];
        auto subscribe = [&](float value)
        {
            int size = tosc_writeMessage(data, sizeof(data), "/subscribe/surface/1", "f", value);
            sendto(listener, data, size, 0, reinterpret_cast<sockaddr*>(&address), sizeof(address));
            server.UdpHandleCommunication();
            REQUIRE(recv(listener, data, sizeof(data), 0) > 0);
        };
        config.SurfaceBind(SurfaceElementId::CLEAR_SOLO, MixerparameterAction::TOGGLE, MP_ID::CHANNEL_MUTE, 0);
        auto* binding = config.GetSurfaceBinding(SurfaceElementId::CLEAR_SOLO);
        auto* element = config.GetSurfaceElement(SurfaceElementId::CLEAR_SOLO);
        REQUIRE(element != nullptr);
        subscribe(1);
        for (float value : {1.0f, 0.0f})
        {
            config.surfaceObserver(element, "press", value);
            int size = recv(listener, data, sizeof(data), 0);
            REQUIRE(size > 0);
            tosc_message message;
            REQUIRE(tosc_parseMessage(&message, data, size) == 0);
            CHECK(std::string(tosc_getAddress(&message)) == "/press/clear_solo/1");
            CHECK(tosc_getNextFloat(&message) == value);
            CHECK(config.GetSurfaceBinding(SurfaceElementId::CLEAR_SOLO) == binding);
            CHECK(binding->mp_action == MixerparameterAction::TOGGLE);
        }
        subscribe(0);
        config.surfaceObserver(element, "press", 1);
        CHECK(recv(listener, data, sizeof(data), 0) < 0);
        close(listener);
        close(server.UdpHandle);
    }
}
