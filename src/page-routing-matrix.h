#pragma once

#include "page.h"

using namespace std;

namespace OMC
{

// Routing as crosspoint grids. Encoder 1 picks a device, its ports are the columns.
//
//   Inputs:  rows are the DSP channels (Ch 1-32, Aux 1-8), columns the inputs of the
//            selected source device. One crosspoint per row.
//   Outputs: rows are the signals (mixer outputs, channel direct outs, FX, physical inputs),
//            columns the outputs of the selected output device. One crosspoint per column,
//            a signal can feed any number of ports on any number of devices.
//            DSP signals reach the FPGA through the 40 DSP returns, which are assigned
//            automatically and shared by ports with the same signal, tap point and delay.
//            The 24 DSP1 -> DSP2 feeds are the output device "DSP2".
//
// The layout lives in OpenX32Gui.eez-project (user widgets routing_xp_header and routing_xp_row);
// this code only fills it. Everything is reachable with the six display encoders.

// The port labels h1..h16 of a header are used as one array
static_assert(offsetof(objects_t, routing_in_header__h16) - offsetof(objects_t, routing_in_header__h1) == 15 * sizeof(lv_obj_t*));
static_assert(offsetof(objects_t, routing_out_header__h16) - offsetof(objects_t, routing_out_header__h1) == 15 * sizeof(lv_obj_t*));

// A signal: a DSP buffer index (dsp) or an FPGA input index. value 0 is off.
struct RoutingSignal
{
    bool dsp = false;
    uint value = 0;

    bool IsOff() const { return value == 0; }
    bool operator==(const RoutingSignal& o) const { return dsp == o.dsp && value == o.value; }
};

// A group of signals with their short labels, e.g. "Card" with "01".."32"
struct RoutingDevice
{
    String name;
    String prefix;           // prefix for long names, e.g. "Card " -> "Card 05"
    vector<RoutingSignal> signals;
    vector<String> labels;
};

class PageRoutingMatrix : public Page
{
    protected:
        using enum MP_ID;

        static constexpr int ROW_H = 22;      // row height of the EEZ user widget routing_xp_row
        static constexpr int COLUMNS = 16;    // crosspoint columns in routing_xp_row / routing_xp_header

        static inline const char* TAP_NAMES[] = { "Input", "Pre EQ", "Post EQ", "Pre Fader", "Post Fader" };

        int dev = 0;          // selected device
        int row = 0;          // selected row
        int col = 0;          // selected port
        int top = 0;          // first visible row
        int left = 0;         // first visible port
        String message;       // shown in red in the info line after a change that could not be applied

        // Values written during the current action. In OSC client mode config->Set only sends to the
        // server, so reading a value back right after setting it would still return the old one.
        map<pair<MP_ID, uint>, float> written;

        float Read(MP_ID mp, uint index)
        {
            auto it = written.find({ mp, index });
            return it != written.end() ? it->second : config->GetFloat(mp, index);
        }

        uint ReadUint(MP_ID mp, uint index)
        {
            return (uint)Read(mp, index);
        }

        void Write(MP_ID mp, float value, uint index)
        {
            written[{ mp, index }] = value;
            config->Set(mp, value, index);
        }

        // EEZ widgets, set by the subclass constructor
        lv_obj_t* devicesBox = nullptr;     // one child per device, each with a label
        lv_obj_t* rowsBox = nullptr;        // routing_xp_row instances: name, sub, COLUMNS cells
        lv_obj_t* scrollThumb = nullptr;
        lv_obj_t* infoLeft = nullptr;
        lv_obj_t* infoRight = nullptr;
        lv_obj_t* dest = nullptr;
        lv_obj_t** headers = nullptr;       // COLUMNS port labels

        //###################################################
        //# Implemented by each grid
        //###################################################

        virtual int DeviceCount() = 0;
        virtual int PortCount() = 0;
        virtual String PortLabel(int p) = 0;
        virtual int RowCount() = 0;
        virtual String RowName(int r) = 0;
        virtual String RowSub(int r) { return ""; }
        virtual bool Connected(int r, int p) = 0;
        virtual bool Connect(int r, int p) = 0;         // false + message when it cannot be applied
        virtual void Disconnect(int r, int p) = 0;
        virtual String Info() = 0;
        virtual String InfoRight() { return ""; }
        // tap point and delay of the selected route
        virtual bool HasTap() = 0;
        virtual uint GetTap() = 0;
        virtual bool SetTap(uint tap) = 0;
        virtual bool HasDelay() = 0;
        virtual float GetDelay() = 0;
        virtual bool ChangeDelay(int amount) = 0;
        virtual String Encoder6Label() = 0;
        virtual void Encoder6(int amount) = 0;
        virtual vector<MP_ID> Watched() = 0;

        //###################################################
        //# Signal catalog
        //###################################################

        static String Pad2(uint n)
        {
            return n < 10 ? String("0") + String(n) : String(n);
        }

        static RoutingDevice Device(String name, String prefix)
        {
            RoutingDevice d;
            d.name = name;
            d.prefix = prefix;
            return d;
        }

        static void Add(RoutingDevice& d, bool dsp, uint value, String label)
        {
            d.signals.push_back({ dsp, value });
            d.labels.push_back(label);
        }

        static void AddRange(RoutingDevice& d, bool dsp, uint first, uint count, String prefix, bool pad)
        {
            for (uint i = 0; i < count; i++)
            {
                Add(d, dsp, first + i, prefix + (pad ? Pad2(i + 1) : String(i + 1)));
            }
        }

        static vector<RoutingDevice> PhysicalInputs()
        {
            vector<RoutingDevice> v;

            RoutingDevice local = Device("Local", "Local ");
            AddRange(local, false, FPGA_INPUT_IDX_XLR, 32, "", true);
            v.push_back(local);

            RoutingDevice card = Device("Card", "Card ");
            AddRange(card, false, FPGA_INPUT_IDX_CARD, 32, "", true);
            v.push_back(card);

            RoutingDevice aux = Device("Aux In", "Aux In ");
            AddRange(aux, false, FPGA_INPUT_IDX_AUX, 6, "", false);
            Add(aux, false, FPGA_INPUT_IDX_TALKBACK_INT, "TB int");
            Add(aux, false, FPGA_INPUT_IDX_TALKBACK_EXT, "TB ext");
            v.push_back(aux);

            RoutingDevice aesA = Device("AES50 A", "AES50 A ");
            AddRange(aesA, false, FPGA_INPUT_IDX_AES50A, 48, "", true);
            v.push_back(aesA);

            RoutingDevice aesB = Device("AES50 B", "AES50 B ");
            AddRange(aesB, false, FPGA_INPUT_IDX_AES50B, 48, "", true);
            v.push_back(aesB);

            return v;
        }

        static RoutingDevice MixerSignals()
        {
            RoutingDevice d = Device("Mixer", "");
            Add(d, true, DSP_BUF_IDX_MAINLEFT, "Main L");
            Add(d, true, DSP_BUF_IDX_MAINRIGHT, "Main R");
            Add(d, true, DSP_BUF_IDX_MAINSUB, "Sub");
            AddRange(d, true, DSP_BUF_IDX_MIXBUS, 16, "Mix ", false);
            AddRange(d, true, DSP_BUF_IDX_MATRIX, 6, "Mtx ", false);
            Add(d, true, DSP_BUF_IDX_MONLEFT, "Mon L");
            Add(d, true, DSP_BUF_IDX_MONRIGHT, "Mon R");
            Add(d, true, DSP_BUF_IDX_TALKBACK, "TB");
            return d;
        }

        static RoutingDevice ChannelSignals()
        {
            RoutingDevice d = Device("Channels", "");
            AddRange(d, true, DSP_BUF_IDX_DSPCHANNEL, 32, "Ch ", true);
            AddRange(d, true, DSP_BUF_IDX_AUX, 8, "Aux ", false);
            return d;
        }

        static RoutingDevice FxSignals()
        {
            RoutingDevice d = Device("FX", "FX ");
            AddRange(d, true, DSP_BUF_IDX_DSP2_FXRET, 8, "Ret ", false);
            AddRange(d, true, DSP_BUF_IDX_DSP2_FXINS, 8, "Ins ", false);
            AddRange(d, true, DSP_BUF_IDX_DSP2_AUX, 8, "Aux ", false);
            return d;
        }

        // "Card 05", "Main L", "FX Ret 1" or "Off"
        static String SignalName(const vector<RoutingDevice>& catalog, RoutingSignal s)
        {
            if (s.IsOff())
            {
                return "Off";
            }
            for (auto const& d : catalog)
            {
                for (size_t i = 0; i < d.signals.size(); i++)
                {
                    if (d.signals[i] == s)
                    {
                        return d.prefix + d.labels[i];
                    }
                }
            }
            return "?";
        }

        static String DelayText(float ms)
        {
            return String(ms, 1) + " ms";
        }

        void SetDeviceLabel(int i, const char* text)
        {
            lv_label_set_text(lv_obj_get_child(lv_obj_get_child(devicesBox, i), 0), text);
        }

        //###################################################
        //# Rendering
        //###################################################

        void Render()
        {
            int devices = DeviceCount();
            for (int i = 0; i < (int)lv_obj_get_child_count(devicesBox); i++)
            {
                lv_obj_t* button = lv_obj_get_child(devicesBox, i);
                lv_obj_set_flag(button, LV_OBJ_FLAG_HIDDEN, i >= devices);
                lv_obj_set_state(button, LV_STATE_CHECKED, i == dev);
                lv_obj_set_state(lv_obj_get_child(button, 0), LV_STATE_CHECKED, i == dev);
            }

            int rows = RowCount();
            int ports = PortCount();
            int visible = lv_obj_get_child_count(rowsBox);
            row = std::clamp(row, 0, rows - 1);
            col = std::clamp(col, 0, ports - 1);
            top = std::clamp(top, std::max(0, row - visible + 1), row);
            top = std::clamp(top, 0, std::max(0, rows - visible));
            left = std::clamp(left, std::max(0, col - COLUMNS + 1), col);
            left = std::clamp(left, 0, std::max(0, ports - COLUMNS));

            for (int j = 0; j < COLUMNS; j++)
            {
                int p = left + j;
                lv_label_set_text(headers[j], p < ports ? PortLabel(p).c_str() : "");
                lv_obj_set_state(headers[j], LV_STATE_FOCUSED, p == col);
            }

            for (int i = 0; i < visible; i++)
            {
                lv_obj_t* rowObj = lv_obj_get_child(rowsBox, i);
                int r = top + i;
                lv_obj_set_flag(rowObj, LV_OBJ_FLAG_HIDDEN, r >= rows);
                if (r >= rows)
                {
                    continue;
                }

                lv_obj_set_state(rowObj, LV_STATE_CHECKED, r == row);
                lv_label_set_text(lv_obj_get_child(rowObj, 0), RowName(r).c_str());
                lv_label_set_text(lv_obj_get_child(rowObj, 1), RowSub(r).c_str());

                for (int j = 0; j < COLUMNS; j++)
                {
                    lv_obj_t* cell = lv_obj_get_child(rowObj, 2 + j);
                    int p = left + j;
                    lv_obj_set_flag(cell, LV_OBJ_FLAG_HIDDEN, p >= ports);
                    if (p < ports)
                    {
                        lv_obj_set_state(cell, LV_STATE_CHECKED, Connected(r, p));
                        lv_obj_set_state(cell, LV_STATE_FOCUSED, r == row && p == col);
                    }
                }
            }

            int trackH = visible * ROW_H;
            int thumbH = rows > visible ? std::max(12, trackH * visible / rows) : trackH;
            int thumbY = rows > visible ? (trackH - thumbH) * top / (rows - visible) : 0;
            lv_obj_set_y(scrollThumb, lv_obj_get_y(rowsBox) + thumbY);
            lv_obj_set_height(scrollThumb, thumbH);

            lv_label_set_text(infoLeft, Info().c_str());
            String right = String("Ports ") + String(left + 1) + "-" + String(std::min(left + COLUMNS, ports)) + " / " + String(ports);
            if (InfoRight().length() > 0)
            {
                right += String("   ") + InfoRight();
            }
            lv_label_set_text(infoRight, message.length() > 0 ? message.c_str() : right.c_str());
            lv_obj_set_style_text_color(infoRight, lv_color_hex(message.length() > 0 ? 0xff5252 : 0x9a9a9a), 0);

            UpdateEncoderLabels();
        }

        //###################################################
        //# Encoders
        //###################################################

        virtual String DeviceName(int d) = 0;

        String EncoderLabel(uint i)
        {
            switch (i)
            {
                case 0: return String("Device: ") + DeviceName(dev);
                case 1: return String(LV_SYMBOL_UP LV_SYMBOL_DOWN " ") + RowName(row);
                case 2: return String(LV_SYMBOL_LEFT LV_SYMBOL_RIGHT " Port ") + PortLabel(col);
                case 3: return String("Tap: ") + (HasTap() ? TAP_NAMES[GetTap()] : "-");
                case 4: return String("Delay: ") + (HasDelay() ? DelayText(GetDelay()) : String("-"));
                default: return Encoder6Label();
            }
        }

        String ButtonLabel(uint i)
        {
            switch (i)
            {
                case 2: return Connected(row, col) ? "Disconnect" : "Connect";
                case 5: return "Patch 8";
                default: return "";
            }
        }

        void UpdateEncoderLabels()
        {
            // mute group and utility mode bind the encoders themselves
            if (config->GetBool(DISPLAY_MUTE_GROUP) || config->GetBool(DISPLAY_UTILITY))
            {
                return;
            }

            // update in place: rebinding would allocate a new binding on every turn
            for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
            {
                SurfaceBindingParameter* encoder = config->GetSurfaceBinding(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, i));
                if (encoder != nullptr && encoder->mp_action == MixerparameterAction::CUSTOM)
                {
                    encoder->custom_label = EncoderLabel(i);
                }
                SurfaceBindingParameter* button = config->GetSurfaceBinding(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, i));
                if (button != nullptr && button->mp_action == MixerparameterAction::CUSTOM)
                {
                    button->custom_label = ButtonLabel(i);
                }
            }
        }

        int EncoderIndex(SurfaceElementId id, SurfaceElementId first)
        {
            for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
            {
                if (config->CalcSurfaceElementId(first, i) == id)
                {
                    return i;
                }
            }
            return -1;
        }

        // Connect row+k to port+k for k = 0..7, e.g. Mix 1-8 to XLR Out 1-8
        void Patch8()
        {
            for (int k = 0; k < 8 && row + k < RowCount() && col + k < PortCount(); k++)
            {
                if (!Connected(row + k, col + k) && !Connect(row + k, col + k))
                {
                    break;
                }
            }
        }

    public:
        PageRoutingMatrix(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            tabLayer0 = objects.maintab;
            tabIndex0 = 2;
            tabLayer1 = objects.routingtab;
        }

        void OnShow() override
        {
            for (uint i = 0; i < MAX_DISPLAY_ENCODER; i++)
            {
                config->SurfaceBindCustom(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_1, i));
                if (ButtonLabel(i).length() > 0)
                {
                    config->SurfaceBindCustom(config->CalcSurfaceElementId(SurfaceElementId::DISPLAY_ENCODER_BUTTON_1, i));
                }
            }
            message = "";
            Render();
        }

        void OnChange(bool force) override
        {
            if (force || config->HasParametersChanged(Watched()))
            {
                Render();
            }
        }

        void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            message = "";
            written.clear();
            switch (EncoderIndex(surface_element_id, SurfaceElementId::DISPLAY_ENCODER_1))
            {
                case 0:
                    dev = std::clamp(dev + amount, 0, DeviceCount() - 1);
                    col = 0;
                    left = 0;
                    break;
                case 1:
                    row += amount;
                    break;
                case 2:
                    col += amount;
                    break;
                case 3:
                    if (HasTap())
                    {
                        SetTap(std::clamp((int)GetTap() + amount, 0, 4));
                    }
                    break;
                case 4:
                    if (HasDelay())
                    {
                        ChangeDelay(amount);
                    }
                    break;
                case 5:
                    Encoder6(amount);
                    break;
                default:
                    break;
            }
            Render();
        }

        void OnChangeCustomButton(SurfaceElementId surface_element_id) override
        {
            message = "";
            written.clear();
            switch (EncoderIndex(surface_element_id, SurfaceElementId::DISPLAY_ENCODER_BUTTON_1))
            {
                case 2:
                    if (Connected(row, col))
                    {
                        Disconnect(row, col);
                    }
                    else
                    {
                        Connect(row, col);
                    }
                    break;
                case 5:
                    Patch8();
                    break;
                default:
                    break;
            }
            Render();
        }
};

//#####################################################################################################################
//# Inputs: DSP channels <- inputs of the selected source device
//#####################################################################################################################

class PageRoutingInputs : public PageRoutingMatrix
{
    private:
        vector<RoutingDevice> devices;

        // FPGA output feeding the FPGA->DSP slot of channel r (slot r+1 = FPGA output 73+r)
        static uint Slot(uint r) { return (FPGA_OUTPUT_IDX_DSP - 1) + r; }

        // Signal of channel r. Slots with a tap other than Input take another channel's
        // processed signal, which no device offers, so they match no column.
        RoutingSignal Source(int r)
        {
            uint v = ReadUint(ROUTING_DSP_INPUT, r);
            if (v == DSP_BUF_IDX_OFF)
            {
                return {};
            }
            if (v < DSP_BUF_IDX_DSP2_FXRET)
            {
                if (ReadUint(ROUTING_DSP_INPUT_TAPPOINT, r) != (uint)DSP_TAP::INPUT)
                {
                    return { true, v };
                }
                return { false, ReadUint(ROUTING_FPGA, Slot(v - 1)) };
            }
            return { true, v };
        }

        // Move every channel that reads another channel's FPGA->DSP slot onto its own slot with the
        // same source, so writing a channel's slot changes only that channel. Audio does not change.
        void UseOwnSlots()
        {
            uint source[MAX_FPGA_TO_DSP1_CHANNELS];
            bool move[MAX_FPGA_TO_DSP1_CHANNELS] = {};
            for (uint j = 0; j < MAX_FPGA_TO_DSP1_CHANNELS; j++)
            {
                uint v = ReadUint(ROUTING_DSP_INPUT, j);
                if (v >= DSP_BUF_IDX_DSPCHANNEL && v < DSP_BUF_IDX_DSP2_FXRET && v != j + 1 &&
                    ReadUint(ROUTING_DSP_INPUT_TAPPOINT, j) == (uint)DSP_TAP::INPUT)
                {
                    move[j] = true;
                    source[j] = ReadUint(ROUTING_FPGA, Slot(v - 1));
                }
            }
            for (uint j = 0; j < MAX_FPGA_TO_DSP1_CHANNELS; j++)
            {
                if (move[j])
                {
                    Write(ROUTING_FPGA, source[j], Slot(j));
                    Write(ROUTING_DSP_INPUT, DSP_BUF_IDX_DSPCHANNEL + j, j);
                }
            }
        }

    protected:
        int DeviceCount() override { return devices.size(); }
        String DeviceName(int d) override { return devices[d].name; }
        int PortCount() override { return devices[dev].signals.size(); }
        String PortLabel(int p) override { return devices[dev].labels[p]; }

        int RowCount() override { return MAX_FPGA_TO_DSP1_CHANNELS; }
        String RowName(int r) override { return r < 32 ? String("Ch ") + Pad2(r + 1) : String("Aux ") + String(r - 31); }
        String RowSub(int r) override { return config->GetString(CHANNEL_NAME, r); }

        bool Connected(int r, int p) override
        {
            return Source(r) == devices[dev].signals[p];
        }

        bool Connect(int r, int p) override
        {
            RoutingSignal s = devices[dev].signals[p];
            if (!s.dsp)
            {
                // every channel uses its own FPGA->DSP slot
                UseOwnSlots();
                Write(ROUTING_FPGA, s.value, Slot(r));
                Write(ROUTING_DSP_INPUT, DSP_BUF_IDX_DSPCHANNEL + r, r);
                Write(ROUTING_DSP_INPUT_TAPPOINT, (uint)DSP_TAP::INPUT, r);
            }
            else
            {
                if (!Source(r).dsp)
                {
                    Write(ROUTING_DSP_INPUT_TAPPOINT, (uint)DSP_TAP::POST_FADER, r);
                }
                Write(ROUTING_DSP_INPUT, s.value, r);
            }
            return true;
        }

        void Disconnect(int r, int p) override
        {
            Write(ROUTING_DSP_INPUT, DSP_BUF_IDX_OFF, r);
        }

        String Info() override
        {
            String text = RowName(row);
            if (RowSub(row).length() > 0)
            {
                text += String(" ") + RowSub(row);
            }
            RoutingSignal s = Source(row);
            String source = SignalName(devices, s);
            if (source == "?")
            {
                source = config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(row);
            }
            text += String("   " LV_SYMBOL_LEFT "   ") + source;
            if (HasTap())
            {
                text += String(", ") + TAP_NAMES[GetTap()];
            }
            return text;
        }

        bool HasTap() override { return Source(row).dsp; }
        uint GetTap() override { return ReadUint(ROUTING_DSP_INPUT_TAPPOINT, row); }
        bool SetTap(uint tap) override { Write(ROUTING_DSP_INPUT_TAPPOINT, tap, row); return true; }
        bool HasDelay() override { return true; }
        float GetDelay() override { return config->GetFloat(DELAY_DSP_INPUT, row); }
        bool ChangeDelay(int amount) override { config->Change(DELAY_DSP_INPUT, amount, row); return true; }

        String Encoder6Label() override { return String(LV_SYMBOL_UP LV_SYMBOL_DOWN " 8 rows"); }
        void Encoder6(int amount) override { row += amount < 0 ? -8 : 8; }

        vector<MP_ID> Watched() override
        {
            return { ROUTING_FPGA, ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT, DELAY_DSP_INPUT, CHANNEL_NAME };
        }

    public:
        PageRoutingInputs(PageBaseParameter* pagebasepar) : PageRoutingMatrix(pagebasepar)
        {
            prevPage = X32_PAGE::NONE;
            nextPage = X32_PAGE::ROUTING_OUTPUTS;
            tabIndex1 = 0;

            devices = PhysicalInputs();
            RoutingDevice buses = MixerSignals();
            buses.name = "DSP";
            devices.push_back(buses);
            devices.push_back(FxSignals());

            devicesBox = objects.routing_in_devices;
            rowsBox = objects.routing_in_rows;
            scrollThumb = objects.routing_in_scroll;
            infoLeft = objects.routing_in_header__info_left;
            infoRight = objects.routing_in_header__info_right;
            dest = objects.routing_in_header__dest;
            headers = &objects.routing_in_header__h1;

            for (int i = 0; i < (int)devices.size(); i++)
            {
                SetDeviceLabel(i, devices[i].name.c_str());
            }
            lv_label_set_text(dest, "Channel");
        }
};

//#####################################################################################################################
//# Outputs: signals -> outputs of the selected output device
//#####################################################################################################################

class PageRoutingOutputs : public PageRoutingMatrix
{
    private:
        struct OutputDevice
        {
            const char* name;
            const char* prefix;
            bool dsp2;          // DSP1 -> DSP2 feeds instead of FPGA outputs
            uint base;          // index into ROUTING_FPGA (FPGA output index - 1) or ROUTING_DSP_OUTPUT
            uint count;
        };

        static inline const OutputDevice OUTPUT_DEVICES[] = {
            { "XLR Out",  "XLR Out ",  false, FPGA_OUTPUT_IDX_XLR - 1,      16 },
            { "Ultranet", "Ultranet ", false, FPGA_OUTPUT_IDX_ULTRANET - 1, 16 },
            { "Aux / CR", "Aux Out ",  false, FPGA_OUTPUT_IDX_AUX - 1,       8 },
            { "Card",     "Card Out ", false, FPGA_OUTPUT_IDX_CARD - 1,     32 },
            { "AES50 A",  "AES50 A ",  false, FPGA_OUTPUT_IDX_AES50A - 1,   48 },
            { "AES50 B",  "AES50 B ",  false, FPGA_OUTPUT_IDX_AES50B - 1,   48 },
            { "DSP2",     "DSP2 ",     true,  MAX_DSP1_TO_FPGA_CHANNELS,    MAX_DSP1_TO_DSP2_CHANNELS },
        };
        static constexpr int OUTPUT_DEVICE_COUNT = std::size(OUTPUT_DEVICES);

        // rows: the signal groups, flattened
        vector<RoutingDevice> groups;
        vector<int> groupOf;
        vector<int> indexInGroup;

        const OutputDevice& Out() { return OUTPUT_DEVICES[dev]; }
        RoutingSignal RowSignal(int r) { return groups[groupOf[r]].signals[indexInGroup[r]]; }

        // DSP->FPGA return (0..39) feeding FPGA output fpga, or -1
        int Return(uint fpga)
        {
            uint s = ReadUint(ROUTING_FPGA, fpga);
            if (s >= FPGA_INPUT_IDX_DSP_RETURN && s < FPGA_INPUT_IDX_DSP_RETURN + MAX_DSP1_TO_FPGA_CHANNELS)
            {
                return s - FPGA_INPUT_IDX_DSP_RETURN;
            }
            return -1;
        }

        RoutingSignal PortSignal(int p)
        {
            if (Out().dsp2)
            {
                return { true, ReadUint(ROUTING_DSP_OUTPUT, Out().base + p) };
            }
            uint fpga = Out().base + p;
            int ret = Return(fpga);
            if (ret >= 0)
            {
                return { true, ReadUint(ROUTING_DSP_OUTPUT, ret) };
            }
            return { false, ReadUint(ROUTING_FPGA, fpga) };
        }

        void CountReturnUsers(uint users[MAX_DSP1_TO_FPGA_CHANNELS])
        {
            for (uint i = 0; i < MAX_DSP1_TO_FPGA_CHANNELS; i++)
            {
                users[i] = 0;
            }
            for (uint i = 0; i < NUM_OUTPUT_CHANNEL; i++)
            {
                int ret = Return(i);
                if (ret >= 0)
                {
                    users[ret]++;
                }
            }
        }

        bool ReturnCarries(int ret, uint buf, uint tap, float delay)
        {
            return ReadUint(ROUTING_DSP_OUTPUT, ret) == buf &&
                   ReadUint(ROUTING_DSP_OUTPUT_TAPPOINT, ret) == tap &&
                   fabsf(Read(DELAY_DSP_OUTPUT, ret) - delay) < 0.01f;
        }

        // Point all FPGA outputs of returns that carry the same signal as an earlier return to that
        // earlier one (the audio does not change), so the later returns become free. Older configs and
        // the defaults use one return per output even for identical signals (e.g. Main L on many outputs).
        bool MergeDuplicateReturns()
        {
            uint users[MAX_DSP1_TO_FPGA_CHANNELS];
            CountReturnUsers(users);
            bool merged = false;
            for (int ret = 1; ret < MAX_DSP1_TO_FPGA_CHANNELS; ret++)
            {
                if (users[ret] == 0)
                {
                    continue;
                }
                for (int first = 0; first < ret; first++)
                {
                    if (users[first] > 0 && ReturnCarries(first,
                            ReadUint(ROUTING_DSP_OUTPUT, ret),
                            ReadUint(ROUTING_DSP_OUTPUT_TAPPOINT, ret),
                            Read(DELAY_DSP_OUTPUT, ret)))
                    {
                        for (uint i = 0; i < NUM_OUTPUT_CHANNEL; i++)
                        {
                            if (Return(i) == ret)
                            {
                                Write(ROUTING_FPGA, FPGA_INPUT_IDX_DSP_RETURN + first, i);
                            }
                        }
                        users[first] += users[ret];
                        users[ret] = 0;
                        merged = true;
                        break;
                    }
                }
            }
            return merged;
        }

        // Route a DSP signal to an FPGA output: reuse a return that already carries it,
        // else reprogram the output's own return if nothing else uses it, else take a free one,
        // else free returns by merging duplicates.
        bool Assign(uint fpga, uint buf, uint tap, float delay)
        {
            uint users[MAX_DSP1_TO_FPGA_CHANNELS];
            CountReturnUsers(users);
            int current = Return(fpga);

            if (current >= 0 && ReturnCarries(current, buf, tap, delay))
            {
                return true;
            }

            int target = -1;
            for (int ret = 0; ret < MAX_DSP1_TO_FPGA_CHANNELS && target < 0; ret++)
            {
                if (users[ret] > 0 && ReturnCarries(ret, buf, tap, delay))
                {
                    target = ret;
                }
            }
            if (target < 0 && current >= 0 && users[current] == 1)
            {
                target = current;
            }
            for (int ret = 0; ret < MAX_DSP1_TO_FPGA_CHANNELS && target < 0; ret++)
            {
                if (users[ret] == 0)
                {
                    target = ret;
                }
            }
            if (target < 0 && MergeDuplicateReturns())
            {
                CountReturnUsers(users);
                current = Return(fpga);
                if (current >= 0 && users[current] == 1)
                {
                    target = current;
                }
                for (int ret = 0; ret < MAX_DSP1_TO_FPGA_CHANNELS && target < 0; ret++)
                {
                    if (users[ret] == 0)
                    {
                        target = ret;
                    }
                }
            }
            if (target < 0)
            {
                message = "No free DSP return (40 in use)";
                return false;
            }

            Write(ROUTING_DSP_OUTPUT, buf, target);
            Write(ROUTING_DSP_OUTPUT_TAPPOINT, tap, target);
            Write(DELAY_DSP_OUTPUT, delay, target);
            Write(ROUTING_FPGA, FPGA_INPUT_IDX_DSP_RETURN + target, fpga);
            return true;
        }

        uint TapIndex() { return Out().dsp2 ? Out().base + col : Return(Out().base + col); }

    protected:
        int DeviceCount() override { return OUTPUT_DEVICE_COUNT; }
        String DeviceName(int d) override { return OUTPUT_DEVICES[d].name; }
        int PortCount() override { return Out().count; }

        String PortLabel(int p) override
        {
            if (!Out().dsp2 && Out().base == FPGA_OUTPUT_IDX_AUX - 1 && p >= 6)
            {
                return p == 6 ? "CR L" : "CR R";
            }
            return Pad2(p + 1);
        }

        String PortName(int p)
        {
            if (Out().dsp2)
            {
                return helper->RoutingGetOutputNameByIndex(Out().base + p + 1);
            }
            if (Out().base == FPGA_OUTPUT_IDX_AUX - 1 && p >= 6)
            {
                return p == 6 ? "Ctrl Room L" : "Ctrl Room R";
            }
            return String(Out().prefix) + Pad2(p + 1);
        }

        int RowCount() override { return groupOf.size(); }
        String RowName(int r) override { return groups[groupOf[r]].prefix + groups[groupOf[r]].labels[indexInGroup[r]]; }

        String RowSub(int r) override
        {
            RoutingSignal s = RowSignal(r);
            if (s.dsp && s.value >= DSP_BUF_IDX_DSPCHANNEL && s.value < DSP_BUF_IDX_DSP2_FXRET)
            {
                return config->GetString(CHANNEL_NAME, s.value - DSP_BUF_IDX_DSPCHANNEL);
            }
            return "";
        }

        bool Connected(int r, int p) override
        {
            RoutingSignal s = PortSignal(p);
            return !s.IsOff() && s == RowSignal(r);
        }

        bool Connect(int r, int p) override
        {
            RoutingSignal s = RowSignal(r);
            if (Out().dsp2)
            {
                if (!s.dsp)
                {
                    message = "DSP2 feeds take DSP signals only";
                    return false;
                }
                Write(ROUTING_DSP_OUTPUT, s.value, Out().base + p);
                Write(ROUTING_DSP_OUTPUT_TAPPOINT, (uint)DSP_TAP::POST_FADER, Out().base + p);
                return true;
            }
            uint fpga = Out().base + p;
            if (!s.dsp)
            {
                Write(ROUTING_FPGA, s.value, fpga);
                return true;
            }
            return Assign(fpga, s.value, (uint)DSP_TAP::POST_FADER, 0.0f);
        }

        void Disconnect(int r, int p) override
        {
            if (Out().dsp2)
            {
                Write(ROUTING_DSP_OUTPUT, DSP_BUF_IDX_OFF, Out().base + p);
            }
            else
            {
                Write(ROUTING_FPGA, FPGA_INPUT_IDX_OFF, Out().base + p);
            }
        }

        // the selected port and what it carries
        String Info() override
        {
            RoutingSignal s = PortSignal(col);
            String source = SignalName(groups, s);
            if (source == "?")
            {
                source = Out().dsp2 || s.dsp
                    ? String("DSP ") + String(s.value)
                    : config->GetParameter(ROUTING_FPGA)->GetFormatedValue(Out().base + col);
            }
            String text = PortName(col) + String("   " LV_SYMBOL_LEFT "   ") + source;
            if (HasTap())
            {
                text += String(", ") + TAP_NAMES[GetTap()];
            }
            if (HasDelay())
            {
                text += String(", ") + DelayText(GetDelay());
            }
            return text;
        }

        String InfoRight() override
        {
            uint users[MAX_DSP1_TO_FPGA_CHANNELS];
            CountReturnUsers(users);
            uint used = 0;
            for (uint i = 0; i < MAX_DSP1_TO_FPGA_CHANNELS; i++)
            {
                used += users[i] > 0 ? 1 : 0;
            }
            return String("Returns ") + String(used) + "/" + String(MAX_DSP1_TO_FPGA_CHANNELS);
        }

        bool HasTap() override
        {
            RoutingSignal s = PortSignal(col);
            return s.dsp && !s.IsOff();
        }

        uint GetTap() override { return ReadUint(ROUTING_DSP_OUTPUT_TAPPOINT, TapIndex()); }

        bool SetTap(uint tap) override
        {
            if (Out().dsp2)
            {
                Write(ROUTING_DSP_OUTPUT_TAPPOINT, tap, TapIndex());
                return true;
            }
            return Assign(Out().base + col, PortSignal(col).value, tap, GetDelay());
        }

        bool HasDelay() override { return !Out().dsp2 && HasTap(); }
        float GetDelay() override { return Read(DELAY_DSP_OUTPUT, TapIndex()); }

        bool ChangeDelay(int amount) override
        {
            Mixerparameter* p = config->GetParameter(DELAY_DSP_OUTPUT);
            float delay = std::clamp(GetDelay() + amount * p->GetStepsize(), p->GetMin(), p->GetMax());
            return Assign(Out().base + col, PortSignal(col).value, GetTap(), delay);
        }

        String Encoder6Label() override { return String("Rows: ") + groups[groupOf[row]].name; }

        // jump to the first row of the next or previous signal group
        void Encoder6(int amount) override
        {
            int g = std::clamp(groupOf[row] + (amount < 0 ? -1 : 1), 0, (int)groups.size() - 1);
            for (int r = 0; r < RowCount(); r++)
            {
                if (groupOf[r] == g)
                {
                    row = r;
                    top = r;
                    break;
                }
            }
        }

        vector<MP_ID> Watched() override
        {
            return { ROUTING_FPGA, ROUTING_DSP_OUTPUT, ROUTING_DSP_OUTPUT_TAPPOINT, DELAY_DSP_OUTPUT, CHANNEL_NAME };
        }

    public:
        PageRoutingOutputs(PageBaseParameter* pagebasepar) : PageRoutingMatrix(pagebasepar)
        {
            prevPage = X32_PAGE::ROUTING;
            nextPage = X32_PAGE::ROUTING_ADVANCED;
            tabIndex1 = 1;

            groups.push_back(MixerSignals());
            groups.push_back(ChannelSignals());
            groups.push_back(FxSignals());
            for (auto const& d : PhysicalInputs())
            {
                groups.push_back(d);
            }
            for (int g = 0; g < (int)groups.size(); g++)
            {
                for (int i = 0; i < (int)groups[g].signals.size(); i++)
                {
                    groupOf.push_back(g);
                    indexInGroup.push_back(i);
                }
            }

            devicesBox = objects.routing_out_devices;
            rowsBox = objects.routing_out_rows;
            scrollThumb = objects.routing_out_scroll;
            infoLeft = objects.routing_out_header__info_left;
            infoRight = objects.routing_out_header__info_right;
            dest = objects.routing_out_header__dest;
            headers = &objects.routing_out_header__h1;

            for (int i = 0; i < OUTPUT_DEVICE_COUNT; i++)
            {
                SetDeviceLabel(i, OUTPUT_DEVICES[i].name);
            }
            lv_label_set_text(dest, "Signal");
        }
};

//#####################################################################################################################
//# Placeholder for the former DSP input page: configs saved on it open the Inputs page
//#####################################################################################################################

class PageRoutingUnused : public Page
{
    protected:
        void OnShow() override
        {
            config->Set(ACTIVE_PAGE, (uint)X32_PAGE::ROUTING);
        }

    public:
        PageRoutingUnused(PageBaseParameter* pagebasepar) : Page(pagebasepar) {}
};

}
