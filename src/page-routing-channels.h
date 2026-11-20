#pragma once

#include "page.h"

using namespace std;

namespace OMC
{

class PageRoutingChannels: public Page
{
#ifndef __clang__
	using enum MP_ID;
#endif
	
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

        vector<uint> GetActiveRoutingChannels()
        {
            vector<uint> list;
            for (uint i = 0; i < 40; i++)
            {
                if (!config->IsRightChannelOfLinkedPair(i))
                {
                    list.push_back(i);
                }
            }
            return list;
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
                
                // Format Destination text
                String inputChannelName;
                if (chanIdx < 40)
                {
                    // Check if this channel is the left of a linked pair
                    bool isLinked = false;
                    uint linkIdx = chanIdx < 32 ? chanIdx / 2 : 16 + (chanIdx - 32) / 2;
                    if (chanIdx % 2 == 0)
                    {
                        isLinked = config->GetBool(CHANNEL_LINKED, linkIdx);
                    }
                    if (isLinked)
                    {
                        if (chanIdx < 32)
                        {
                            inputChannelName = String("Channel ") + (chanIdx + 1) + "/" + (chanIdx + 2);
                        }
                        else
                        {
                            inputChannelName = String("Aux ") + (chanIdx - 32 + 1) + "/" + (chanIdx - 32 + 2);
                        }
                    }
                    else
                    {
                        if (chanIdx < 32)
                        {
                            inputChannelName = String("Channel ") + (chanIdx + 1);
                        }
                        else
                        {
                            inputChannelName = String("Aux ") + (chanIdx - 32 + 1);
                        }
                    }
                }
                
                // Format Source text
                String sourceName;
                if (chanIdx < 40 && chanIdx % 2 == 0 && config->GetBool(CHANNEL_LINKED, chanIdx < 32 ? chanIdx / 2 : 16 + (chanIdx - 32) / 2))
                {
                    // It's a linked stereo pair. Display both left and right sources.
                    uint routingL = config->GetUint(ROUTING_DSP_INPUT, chanIdx);
                    uint routingR = config->GetUint(ROUTING_DSP_INPUT, chanIdx + 1);
                    
                    String srcL_fpga = config->GetParameter(ROUTING_FPGA)->GetFormatedValue(72 + chanIdx);
                    String srcL_dsp = config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(chanIdx);
                    String srcR_fpga = config->GetParameter(ROUTING_FPGA)->GetFormatedValue(72 + chanIdx + 1);
                    String srcR_dsp = config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(chanIdx + 1);
                    
                    String fullL = (routingL >= 1 && routingL <= 40) ? (srcL_fpga + " -> " + srcL_dsp) : srcL_dsp;
                    String fullR = (routingR >= 1 && routingR <= 40) ? (srcR_fpga + " -> " + srcR_dsp) : srcR_dsp;
                    
                    sourceName = fullL + " / " + fullR;
                }
                else
                {
                    // Single channel routing
                    uint routingVal = config->GetUint(ROUTING_DSP_INPUT, chanIdx);
                    String src_fpga = config->GetParameter(ROUTING_FPGA)->GetFormatedValue(72 + chanIdx);
                    String src_dsp = config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(chanIdx);
                    
                    if (routingVal >= 1 && routingVal <= 40)
                    {
                        sourceName = src_fpga + " -> " + src_dsp;
                    }
                    else
                    {
                        sourceName = src_dsp;
                    }
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
            /* Original code:
	 		if(config->HasParametersChanged({ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT}) || force)
	 		{
	 			for(auto const& index : config->GetChangedParameterIndexes({ROUTING_DSP_INPUT, ROUTING_DSP_INPUT_TAPPOINT}))
                {
	 				if ((config->GetUint(ROUTING_DSP_INPUT, index) >= 1) && (config->GetUint(ROUTING_DSP_INPUT, index) <= 40)) {
	 					// external signal from FPGA
	 					lv_table_set_cell_value(objects.table_routing_dsp_input, index, 0, (config->GetParameter(ROUTING_FPGA)->GetFormatedValue(FPGA_OUTPUT_IDX_DSP - 1 + index) + " -> " + config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(index)).c_str());
	 				}else{
	 					// internal signal within DSP
	 					lv_table_set_cell_value(objects.table_routing_dsp_input, index, 0, config->GetParameter(ROUTING_DSP_INPUT)->GetFormatedValue(index).c_str());
	 				}
	 				lv_table_set_cell_value(objects.table_routing_dsp_input, index, 2, config->GetParameter(ROUTING_DSP_INPUT_TAPPOINT)->GetFormatedValue(index).c_str());
	 			}
	 		}
            */
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
                    /* Original code:
					int8_t absoluteChange;
					if (amount < 0) {
						absoluteChange = -8;
					}else{
						absoluteChange = 8;
					}
					for (uint8_t i = (gui_selected_item); i < (gui_selected_item + 8); i++) {
						config->Change(ROUTING_DSP_INPUT, absoluteChange, i);
					}
                    */
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
