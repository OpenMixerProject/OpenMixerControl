#pragma once
#include "page.h"
using namespace std;

namespace OMC
{

class PageSetup: public Page
{
    using enum MP_ID;

    private:
        lv_obj_t* usb_status_label = nullptr;
        bool format_confirm_pending = false;
        time_t format_confirm_deadline = 0;

        void UpdateUsbStatusLabel()
        {
            if (usb_status_label == nullptr)
            {
                return;
            }

            String devInfo = config->GetString(USB_STORAGE_DEVICE_INFO);
            String usbState = config->GetParameter(USB_STORAGE_STATE)->GetFormatedValue();
            String targetFs = config->GetParameter(USB_STORAGE_FORMAT_TARGET)->GetFormatedValue();
            bool bootProtected = config->GetBool(USB_STORAGE_BOOT_PROTECTED);

            if (bootProtected)
            {
                lv_label_set_text_fmt(usb_status_label,
                                      "USB: %s [%s]\nFormat Locked: OS Boot USB detected",
                                      devInfo.c_str(), usbState.c_str());
            }
            else if (format_confirm_pending)
            {
                lv_label_set_text_fmt(usb_status_label,
                                      "USB: %s [%s]\nCONFIRM: Press Encoder 3 again to format as %s (ERASES ALL DATA!)",
                                      devInfo.c_str(), usbState.c_str(), targetFs.c_str());
            }
            else
            {
                lv_label_set_text_fmt(usb_status_label,
                                      "USB: %s [%s]\nFormat Target: %s",
                                      devInfo.c_str(), usbState.c_str(), targetFs.c_str());
            }
        }

        void UpdateFormatEncoderLabel()
        {
            if (config->GetBool(USB_STORAGE_BOOT_PROTECTED))
            {
                config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, "Format Locked");
            }
            else if (format_confirm_pending)
            {
                String targetFs = config->GetParameter(USB_STORAGE_FORMAT_TARGET)->GetFormatedValue();
                config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, String("Confirm ") + targetFs);
            }
            else
            {
                config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, "Format USB");
            }
        }

    public:
        PageSetup(PageBaseParameter* pagebasepar) : Page(pagebasepar)
        {
            nextPage = X32_PAGE::SETUP_CARD;
            tabLayer0 = objects.maintab;
            tabIndex0 = 3;
            tabLayer1 = objects.setuptab;
            tabIndex1 = 0;
        }

        void OnInit() override
        {
            if (objects.setup_versioninfotext != nullptr)
            {
                lv_obj_t* parent = lv_obj_get_parent(objects.setup_versioninfotext);
                if (parent != nullptr)
                {
                    usb_status_label = lv_label_create(parent);
                    lv_obj_set_pos(usb_status_label, 32, 160);
                    lv_obj_set_size(usb_status_label, 720, LV_SIZE_CONTENT);
                    UpdateUsbStatusLabel();
                }
            }
        }

        void OnShow() override 
        {
            format_confirm_pending = false;
            format_confirm_deadline = 0;

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_1, MixerparameterAction::CHANGE, USB_STORAGE_FORMAT_TARGET);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_2, MixerparameterAction::CHANGE_SELECTED_CHANNEL, CHANNEL_COLOR);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_BUTTON_2, MixerparameterAction::TOGGLE_SELECTED_CHANNEL, CHANNEL_COLOR_INVERTED);
            UpdateFormatEncoderLabel();

            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_4, MixerparameterAction::CHANGE, LED_BRIGHTNESS);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_5, MixerparameterAction::CHANGE, CHANNEL_LCD_MODE);
            config->SurfaceBind(SurfaceElementId::DISPLAY_ENCODER_6, MixerparameterAction::CHANGE, LCD_CONTRAST);

            UpdateUsbStatusLabel();
        }

        void OnChange(bool force_update) override
        {
            if (format_confirm_pending && std::time(nullptr) > format_confirm_deadline)
            {
                format_confirm_pending = false;
                UpdateFormatEncoderLabel();
                UpdateUsbStatusLabel();
            }

            if (config->HasParameterChanged(USB_STORAGE_STATE) ||
                config->HasParameterChanged(USB_STORAGE_FS_TYPE) ||
                config->HasParameterChanged(USB_STORAGE_DEVICE_INFO) ||
                config->HasParameterChanged(USB_STORAGE_FORMAT_TARGET) ||
                config->HasParameterChanged(USB_STORAGE_BOOT_PROTECTED) ||
                force_update)
            {
                bool confirmCancelled = false;
                if ((config->HasParameterChanged(USB_STORAGE_FORMAT_TARGET) ||
                     config->HasParameterChanged(USB_STORAGE_BOOT_PROTECTED)) && format_confirm_pending)
                {
                    format_confirm_pending = false;
                    confirmCancelled = true;
                }
                if (confirmCancelled || config->HasParameterChanged(USB_STORAGE_BOOT_PROTECTED) || force_update)
                {
                    UpdateFormatEncoderLabel();
                }
                UpdateUsbStatusLabel();
            }
        }

        void OnChangeCustomButton(SurfaceElementId surface_element_id) override
        {
            if (surface_element_id == SurfaceElementId::DISPLAY_ENCODER_BUTTON_3)
            {
                USB_DRIVE_STATE usbState = static_cast<USB_DRIVE_STATE>(config->GetUint(USB_STORAGE_STATE));
                if (usbState == USB_DRIVE_STATE::REMOVED ||
                    usbState == USB_DRIVE_STATE::FORMATTING ||
                    config->GetBool(USB_STORAGE_BOOT_PROTECTED))
                {
                    format_confirm_pending = false;
                    UpdateFormatEncoderLabel();
                    UpdateUsbStatusLabel();
                    return;
                }

                if (!format_confirm_pending)
                {
                    format_confirm_pending = true;
                    format_confirm_deadline = std::time(nullptr) + 5;
                    UpdateFormatEncoderLabel();
                    UpdateUsbStatusLabel();
                }
                else
                {
                    format_confirm_pending = false;
                    UpdateFormatEncoderLabel();
                    config->Set(USB_STORAGE_FORMAT_COMMAND, 1u);
                    UpdateUsbStatusLabel();
                }
            }
        }
};

}