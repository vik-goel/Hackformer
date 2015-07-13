struct FieldSpec {};

#define SAVE_BINARY
#include "hackformer_types.h"
#include "hackformer_renderer.h"

#include "hackformer_renderer.cpp"

s32 getFileSize(char* fileName) {
	FILE* file = fopen(fileName, "rb");
	assert(file);

	fseek(file, 0L, SEEK_END);
	s32 result = ftell(file);

	fclose(file);

	return result;
}

void writeFile(FILE* output, char* inputFileName) {
	s32 fileSize = getFileSize(inputFileName);
	
	FILE* input = fopen(inputFileName, "rb");
	assert(input);

	void* mem = malloc(fileSize + 1);
	assert(mem);

	fread(mem, fileSize, 1, input);
	fclose(input);

	fwrite(mem, fileSize, 1, output);
	free(mem);
}

int main(int argc, char** argv) {
	char* fileNames[] = {
		"shaders/stencil.frag",
		"shaders/forward.vert",
		"shaders/forward.frag",
		"shaders/basic.vert",
		"shaders/basic.frag",

		"fonts/PTS55f.ttf",
		"fonts/Roboto-Regular.ttf",

		"music/hackformer_theme.mp3",
		"music/hackformerlevel0.mp3",

		"attributes/attribute.png",
		"attributes/behaviour.png",
		"attributes/changer_readout.png",
		"attributes/left_button.png",
		"attributes/left_button_clicked.png",
		"attributes/left_button_unavailable.png",

		"backgrounds/marine_city_bg.png",
		"backgrounds/marine_city_mg.png",
		"backgrounds/sunset_city_bg.png",
		"backgrounds/sunset_city_mg.png",

		"dock/accept_button_default.png",
		"dock/accept_button_clicked.png",
		"dock/accept_button_hover.png",
		"dock/cancel_button_default.png",
		"dock/cancel_button_clicked.png",
		"dock/cancel_button_hover.png",
		"dock/bar_energy.png",
		"dock/energy_bar_stencil.png",
		"dock/dock.png",
		"dock/sub_dock.png",
		"dock/gravity_field.png",
		"dock/time_field.png",

		"main_menu/background.png",
		"main_menu/background_animation.png",
		"main_menu/options_button.png",
		"main_menu/play_button.png",
		"main_menu/quit_button.png",

		"mothership/base.png",
		"mothership/emitter.png",
		"mothership/projectile.png",
		"mothership/projectile_death.png",
		"mothership/projectile_smoking.png",
		"mothership/rotator_1.png",
		"mothership/rotator_2.png",
		"mothership/rotator_3.png",
		"mothership/spawning.png",

		"pause_menu/pause_menu.png",
		"pause_menu/pause_menu_sprite.png",
		"pause_menu/quit_button.png",
		"pause_menu/restart_button.png",
		"pause_menu/resume_button.png",
		"pause_menu/settings_button.png",

		"player/death.png",
		"player/hacking.png",
		"player/jumping_intro.png",
		"player/jumping.png",
		"player/jumping_outro.png",
		"player/running.png",
		"player/stand_2.png",

		"shrike/boot_up.png",
		"shrike/jet_burn.png",
		"shrike/shoot.png",
		
		"tile_hacking/corner_tile_hack_shield.png",
		"tile_hacking/tile_hack_shield.png",
		"tile_hacking/right_button.png",

		"tiles/corner_1_regular.png",
		"tiles/corner_1_glowing.png",
		"tiles/disappear_regular.png",
		"tiles/disappear_glowing.png",
		"tiles/heavy_regular.png",
		"tiles/heavy_glowing.png",
		"tiles/middle_1_regular.png",
		"tiles/middle_1_glowing.png",
		"tiles/top_1_regular.png",
		"tiles/top_1_glowing.png",
		"tiles/top_2_regular.png",
		"tiles/top_2_glowing.png",

		"trawler/body.png",
		"trawler/bolt.png",
		"trawler/bolt_death.png",
		"trawler/boot_up.png",
		"trawler/right_frame.png",
		"trawler/shoot.png",
		"trawler/wheel.png",

		"trojan/bolt.png",
		"trojan/bolt_death.png",
		"trojan/disappear.png",
		"trojan/full.png",
		"trojan/shoot.png",
		
		"virus3/base_off.png",
		"virus3/base_on.png",
		"virus3/laser_beam.png",
		"virus3/top_off.png",
		"virus3/top_on.png",

		"waypoints/current_waypoint.png",
		"waypoints/current_waypoint_line.png",
		"waypoints/default_waypoint.png",
		"waypoints/default_waypoint_line.png",
		"waypoints/moved_waypoint.png",
		"waypoints/moved_waypoint_line.png",
		"waypoints/selected_waypoint.png",
		"waypoints/waypoint_arrow.png",

		"circle.png",
		"triangle.png",
		"white.png",

		"cursor_default.png",
		"cursor_hacking.png",

		"end_portal.png",
		"energy_animation.png",
		"light_0.png",
		"light_1.png",
	};

	s32 fileNameCount = arrayCount(fileNames);
	s32 assetCount = Asset_count - 1;
	assert(fileNameCount == assetCount);

	char* outputFileName = "assets.bin";

	FILE* file = fopen(outputFileName, "wb");
	assert(file);

	char* filePaths[arrayCount(fileNames)][1000];

	for(s32 i = 0; i < arrayCount(fileNames); i++) {
		sprintf((char*)filePaths[i], "res/%s", fileNames[i]);
	}

	s32 offset = sizeof(s32) * assetCount;

	for(s32 i = 0; i < assetCount; i++) {
		writeS32(file, offset);
		s32 fileSize = getFileSize((char*)filePaths[i]);
		offset += fileSize;
	}

	for(s32 i = 0; i < assetCount; i++) {
		writeFile(file, (char*)filePaths[i]);
	}

	fclose(file);
	return 0;
} 