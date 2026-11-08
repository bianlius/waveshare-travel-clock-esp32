idf_component_register(SRCS 
"main.c" 

"pic_data/picGroup.c" 
"LCD/backLight.c" 
"NET/connect.c"
"NET/RTC_Internet.c"
"NET/weather_forecast.c" 
"adc.c"
"rom.c"
"mp3_player.c"
"sound_catalog.c"
"sound.c"
"waveshare_audio.c"
"pinyin.c" 

"PAGE/pageDraw.c" 
"PAGE/HOME/home.c" 
"PAGE/HOME/calendar.c" 
"PAGE/HOME/pkmMovie.c" 
"PAGE/SHOP/shop.c"
"PAGE/SHOP/shop_model.c" 
"PAGE/MENU/menu.c" 
"PAGE/WIFI/wifiScanPage.c" 
"PAGE/CITY/city.c" 
"PAGE/TCLOCK/tclock.c" 
"PAGE/SETTING/setting.c" 
"PAGE/SETTING/clockSetting.c" 
"PAGE/WEATHER/weather.c" 
"PAGE/GARDEN/garden.c"
"PAGE/GARDEN/gardenShop.c"
"PAGE/INPUTTXT/inputtxt.c"
"PAGE/INPUTTXT/wifi_keyboard.c"
INCLUDE_DIRS "")

if(GVIEW_UI_AUDIT)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_sources(${COMPONENT_LIB} PRIVATE "../tools/ui_audit.c")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE GVIEW_UI_AUDIT)
endif()
