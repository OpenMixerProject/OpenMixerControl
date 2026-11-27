#pragma once

#include "page.h"

using namespace std;

namespace OMC
{

class PageRoutingChannels: public Page
{
	using enum MP_ID;
	
	private:
	
       	char* button_map[47];
        uint selectionindex = 0;
	
		int gui_selected_item = 0;
		int gui_selected_item_before = 0;
        bool page_routing_dsp1_table_drawn = false;
	
        static void draw_event_header_cb(lv_event_t * e) {
            lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
            lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
            //lv_obj_t* obj = (lv_obj_t*)lv_event_get_target_obj(e);
	
            // if the cells are drawn
            if(base_dsc->part == LV_PART_ITEMS) {
                uint32_t row = base_dsc->id1;
                //uint32_t col = base_dsc->id2;
	
                if(row == 0) {
                    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                    if(label_draw_dsc) {
                        label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
                    }
                    lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
                    if(fill_draw_dsc) {
                        fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), fill_draw_dsc->color, LV_OPA_20);
                        fill_draw_dsc->opa = LV_OPA_COVER;
                    }
				}
			}
		}
	
        static void draw_event_cb(lv_event_t * e) {
            lv_draw_task_t * draw_task = lv_event_get_draw_task(e);
            lv_draw_dsc_base_t * base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);
            lv_obj_t* obj = (lv_obj_t*)lv_event_get_target_obj(e);
	
            // if the cells are drawn
            if(base_dsc->part == LV_PART_ITEMS) {
                uint32_t row = base_dsc->id1;
                //uint32_t col = base_dsc->id2;
	
				/*
                if(row == 0) {
                    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                    if(label_draw_dsc) {
                        label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
                    }
                    lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
                    if(fill_draw_dsc) {
                        fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_BLUE), fill_draw_dsc->color, LV_OPA_20);
                        fill_draw_dsc->opa = LV_OPA_COVER;
                    }
				}
				*/
				/*
                // In the first column align the texts to the right
                else if(col == 0) {
                    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                    if(label_draw_dsc) {
                        label_draw_dsc->align = LV_TEXT_ALIGN_RIGHT;
                    }
                }
				*/
	
				/*
                // Make every 2nd row grayish
                if((row != 0 && row % 2) == 0) {
                    lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
                    if(fill_draw_dsc) {
                        fill_draw_dsc->color = lv_color_mix(lv_palette_main(LV_PALETTE_GREY), fill_draw_dsc->color, LV_OPA_10);
                        //fill_draw_dsc->opa = LV_OPA_COVER;
                    }
                }
				*/
	
				// highlight selected row
                if (row == (*(int*)lv_obj_get_user_data(obj))) {
                    lv_draw_fill_dsc_t * fill_draw_dsc = lv_draw_task_get_fill_dsc(draw_task);
                    if(fill_draw_dsc) {
                        fill_draw_dsc->color = lv_palette_main(LV_PALETTE_BLUE);
                        //fill_draw_dsc->opa = LV_OPA_20;
                    }
	
                    // draw all cells center-aligned
                    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                    if(label_draw_dsc) {
						label_draw_dsc->color = LV_COLOR_MAKE(0x00, 0x00, 0x00);
                        label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
                    }
                }else{
                    // draw all cells center-aligned
                    lv_draw_label_dsc_t * label_draw_dsc = lv_draw_task_get_label_dsc(draw_task);
                    if(label_draw_dsc) {
						label_draw_dsc->color = LV_COLOR_MAKE(0xFA, 0xFA, 0xFA);
                        label_draw_dsc->align = LV_TEXT_ALIGN_CENTER;
                    }
                }
            }
        }

        // DSP input channels shown in the table: the right channel of a stereo pair is part of the left row
        vector<uint> GetActiveRoutingChannels()
        {
            vector<uint> list;
            for (uint i = 0; i < MAX_FPGA_TO_DSP1_CHANNELS; i++)
            {
                if (!config->IsStereoLinkedRight(i))
                {
                    list.push_back(i);
                }
            }
            return list;
        }

        String GetSourceName(uint chanIdx)
        {
            uint routing = config->GetUint(ROUTING_DSP_INPUT, chanIdx);
            String source = config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(chanIdx);

            if (routing >= 1 && routing <= 40)
            {
                // external signal from FPGA
                return config->GetParameter(ROUTING_FPGA)->GetFormatedValue(FPGA_OUTPUT_IDX_DSP - 1 + chanIdx) + " -> " + source;
            }

            // internal signal within DSP
            return source;
        }

        void RebuildTable()
        {
            vector<uint> active = GetActiveRoutingChannels();
            uint rowCount = active.size();
            if (rowCount == 0) return;

            lv_table_set_row_count(objects.table_routing_dsp_input, rowCount);

            for (uint i = 0; i < rowCount; i++)
            {
                uint chanIdx = active[i];
                uint peerIdx;
                bool isLinked = config->GetStereoPeer(chanIdx, peerIdx);

                String inputChannelName = (chanIdx < 32) ? String("Channel ") + (chanIdx + 1) : String("Aux ") + (chanIdx - 32 + 1);
                String sourceName = GetSourceName(chanIdx);
                if (isLinked)
                {
                    inputChannelName += String("/") + ((chanIdx < 32) ? (chanIdx + 2) : (chanIdx - 32 + 2));
                    sourceName += " / " + GetSourceName(peerIdx);
                }

                // Set cell values
                lv_table_set_cell_value(objects.table_routing_dsp_input, i, 0, sourceName.c_str());
                lv_table_set_cell_value(objects.table_routing_dsp_input, i, 1, " ");
                lv_table_set_cell_value(objects.table_routing_dsp_input, i, 2, config->GetParameter(ROUTING_DSP_INPUT_TAPPOINT)->GetFormatedValue(chanIdx).c_str());
                lv_table_set_cell_value(objects.table_routing_dsp_input, i, 3, " ");
                lv_table_set_cell_value(objects.table_routing_dsp_input, i, 4, inputChannelName.c_str());
            }

            // Reapply selection indicators
            if (gui_selected_item >= (int)rowCount)
            {
                gui_selected_item = rowCount - 1;
            }
            if (gui_selected_item < 0)
            {
                gui_selected_item = 0;
            }
            gui_selected_item_before = gui_selected_item;

            lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item, 1, LV_SYMBOL_RIGHT);
            lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item, 3, LV_SYMBOL_RIGHT);
            lv_table_set_selected_cell(objects.table_routing_dsp_input, gui_selected_item, 2);
        }

        void UpdateRowSelection()
        {
            vector<uint> active = GetActiveRoutingChannels();
            int rowCount = (int)active.size();
            if (rowCount == 0) return;

            if (gui_selected_item < 0)
            {
                gui_selected_item = 0;
            }
            else if (gui_selected_item >= rowCount)
            {
                gui_selected_item = rowCount - 1;
            }

            if (gui_selected_item_before < 0)
            {
                gui_selected_item_before = 0;
            }
            else if (gui_selected_item_before >= rowCount)
            {
                gui_selected_item_before = rowCount - 1;
            }

            if(gui_selected_item_before != gui_selected_item)
            {
                // remove old indicator
                lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item_before, 1, " ");
                lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item_before, 3, " ");
			
                // display new indicator
                lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item, 1, LV_SYMBOL_RIGHT);
                lv_table_set_cell_value(objects.table_routing_dsp_input, gui_selected_item, 3, LV_SYMBOL_RIGHT);
			
                // set select to scroll table
                lv_table_set_selected_cell(objects.table_routing_dsp_input, gui_selected_item, 2);
			
                gui_selected_item_before = gui_selected_item;
            }
        }
    
    public:

        PageRoutingChannels(PageBaseParameter* pagebasepar) : Page(pagebasepar)
		{
            prevPage = X32_PAGE::ROUTING_FPGA;
            nextPage = X32_PAGE::ROUTING_DSP2;
            tabLayer0 = objects.maintab;
            tabIndex0 = 2;
            tabLayer1 = objects.routingtab;
            tabIndex1 = 2;
        }

	 	void OnInit() override
	 	{
	 		// Header
            lv_table_set_column_count(objects.table_routing_dsp_input_header, 5);
            lv_table_set_column_width(objects.table_routing_dsp_input_header, 0, 400);
            lv_table_set_column_width(objects.table_routing_dsp_input_header, 1, 50);
            lv_table_set_column_width(objects.table_routing_dsp_input_header, 2, 100);
	 		lv_table_set_column_width(objects.table_routing_dsp_input_header, 3, 50);
	 		lv_table_set_column_width(objects.table_routing_dsp_input_header, 4, 200);
            lv_table_set_cell_value(objects.table_routing_dsp_input_header, 0, 0, "Source");
	 		lv_table_set_cell_value(objects.table_routing_dsp_input_header, 0, 2, "Tappoint");
            lv_table_set_cell_value(objects.table_routing_dsp_input_header, 0, 4, "Destination");

            lv_obj_add_event_cb(objects.table_routing_dsp_input_header, draw_event_header_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
            lv_obj_add_flag(objects.table_routing_dsp_input_header, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

	 		// Selection-Table
	 		lv_table_set_row_count(objects.table_routing_dsp_input, 40); /*Not required but avoids a lot of memory reallocation lv_table_set_set_value*/
            lv_table_set_column_count(objects.table_routing_dsp_input, 5); // Input | # | Source | # | Tap
            lv_table_set_column_width(objects.table_routing_dsp_input, 0, 400);
            lv_table_set_column_width(objects.table_routing_dsp_input, 1, 50);
            lv_table_set_column_width(objects.table_routing_dsp_input, 2, 100);
            lv_table_set_column_width(objects.table_routing_dsp_input, 3, 50);
            lv_table_set_column_width(objects.table_routing_dsp_input, 4, 200);

            RebuildTable();

            lv_obj_add_event_cb(objects.table_routing_dsp_input, draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
            lv_obj_add_flag(objects.table_routing_dsp_input, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);

            // store config pointer in user data for use in draw callback
            lv_obj_set_user_data(objects.table_routing_dsp_input, &gui_selected_item);
	 	}

	 	void OnShow() override
	 	{
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_1, String(LV_SYMBOL_UP) + String("\nSelect\n") + LV_SYMBOL_DOWN);
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_2, String(LV_SYMBOL_UP) + String("\nSelect (8)\n") + LV_SYMBOL_DOWN);

            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_3, String(LV_SYMBOL_REFRESH) + String("\nSource"));
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_4, String(LV_SYMBOL_REFRESH) + String("\nSource (8)"));
            config->SurfaceBindCustom(SurfaceElementId::DISPLAY_ENCODER_5, String(LV_SYMBOL_REFRESH) + String("\nTappoint"));

            RebuildTable();
	 	}

		void OnChange(bool force) override
		{
            if (config->HasParametersChanged({CHANNEL_LINKED, ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT}) || force)
            {
                RebuildTable();
            }
            else
            {
                UpdateRowSelection();
            }
		}

		void OnChangeCustomEncoder(SurfaceElementId surface_element_id, int amount) override
        {
            vector<uint> active = GetActiveRoutingChannels();
            if (active.empty()) return;

            if (gui_selected_item < 0) gui_selected_item = 0;
            if (gui_selected_item >= (int)active.size()) gui_selected_item = active.size() - 1;

            switch (surface_element_id)
            {
                case SurfaceElementId::DISPLAY_ENCODER_1:
					gui_selected_item += amount;
                    UpdateRowSelection();
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_2:
					if (amount < 0) {
						gui_selected_item -= 8;
					}else{
						gui_selected_item += 8;
					}
                    UpdateRowSelection();
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_3:
					config->Change(ROUTING_DSP_INPUT, amount, active[gui_selected_item]);
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_4:
                    {
                        int8_t absoluteChange = (amount < 0) ? -8 : 8;
                        for (uint8_t i = 0; i < 8; i++) {
                            uint targetRow = gui_selected_item + i;
                            if (targetRow < active.size()) {
                                config->Change(ROUTING_DSP_INPUT, absoluteChange, active[targetRow]);
                            }
                        }
                    }
                    break;
                case SurfaceElementId::DISPLAY_ENCODER_5:
					config->Change(ROUTING_DSP_INPUT_TAPPOINT, amount, active[gui_selected_item]);
                    break;
                default:
                    break;
            }
        }
};

}