#include <stdinclude.hpp>
#include <rapidjson/error/en.h>

extern bool init_hook_base();
extern bool init_hook();
extern void uninit_hook();
extern void start_console();

bool g_dump_entries = false;
bool g_dump_localization = false;
bool g_dump_il2cpp = false;
bool g_dump_texture = false;
bool g_enable_logger = false;
bool g_enable_console = false;
int g_max_fps = -1;
bool g_unlock_size = false;
float g_ui_scale = 1.0f;
float g_ui_animation_scale = 1.0f;
float g_resolution_3d_scale = 1.0f;
bool g_character_params_unlimit_ratio = false;
bool g_character_params_is_fixed_ratio = false;
float g_character_param_height_ratio = -1.0f;
float g_character_param_bust_ratio = -1.0f;
float g_character_param_head_ratio = -1.0f;
float g_character_param_arm_ratio = -1.0f;
float g_character_param_hand_ratio = -1.0f;
bool g_replace_to_custom_font = false;
std::string g_font_assetbundle_path;
std::string g_font_asset_name;
std::string g_tmpro_font_asset_name;
int g_anti_aliasing = -1;
std::string g_custom_title_name;
std::unordered_map<std::string, ReplaceAsset> g_replace_assets;
std::string g_replace_assetbundle_file_path;
std::vector<std::string> g_replace_assetbundle_file_paths;

rapidjson::Document localization_dict;

bool has_json_parse_error = false;
std::string json_parse_error_msg;

namespace
{
	void create_debug_console()
	{
		AllocConsole();

		// open stdout stream
		auto _ = freopen("CONOUT$", "w+t", stdout);
		_ = freopen("CONOUT$", "w", stderr);
		_ = freopen("CONIN$", "r", stdin);

		SetConsoleTitle("Song for Prism - Debug Console");

		// set this to avoid turn japanese texts into question mark
		SetConsoleOutputCP(CP_UTF8);
		std::locale::global(std::locale(""));

		wprintf(L"Song for Prism Localify Patch Loaded! - By Kimjio\n");
	}

	std::vector<std::string> read_config()
	{
		std::ifstream config_stream{ "config.json" };
		std::vector<std::string> dicts{};

		if (!config_stream.is_open())
			return dicts;

		rapidjson::IStreamWrapper wrapper{ config_stream };
		rapidjson::Document document;

		document.ParseStream(wrapper);

		if (!document.HasParseError())
		{
			if (document.HasMember("enableConsole"))
			{
				g_enable_console = document["enableConsole"].GetBool();
			}
			if (document.HasMember("enableLogger"))
			{
				g_enable_logger = document["enableLogger"].GetBool();
			}
			if (document.HasMember("dumpStaticEntries"))
			{
				g_dump_entries = document["dumpStaticEntries"].GetBool();
			}
			if (document.HasMember("dumpLocalization"))
			{
				g_dump_localization = document["dumpLocalization"].GetBool();
			}
			if (document.HasMember("dumpIl2Cpp"))
			{
				g_dump_il2cpp = document["dumpIl2Cpp"].GetBool();
			}
			if (document.HasMember("dumpTexture"))
			{
				g_dump_texture = document["dumpTexture"].GetBool();
			}
			if (document.HasMember("maxFps"))
			{
				g_max_fps = document["maxFps"].GetInt();
			}
			if (document.HasMember("unlockSize"))
			{
				g_unlock_size = document["unlockSize"].GetBool();
			}
			if (document.HasMember("uiScale"))
			{
				g_ui_scale = document["uiScale"].GetFloat();
			}
			if (document.HasMember("uiAnimationScale"))
			{
				g_ui_animation_scale = document["uiAnimationScale"].GetFloat();
			}
			if (document.HasMember("resolution3dScale"))
			{
				g_resolution_3d_scale = document["resolution3dScale"].GetFloat();
			}
			if (document.HasMember("characterParamsUnlimitRatio"))
			{
				g_character_params_unlimit_ratio = document["characterParamsUnlimitRatio"].GetBool();
			}
			if (document.HasMember("characterParamsIsFixedRatio"))
			{
				g_character_params_is_fixed_ratio = document["characterParamsIsFixedRatio"].GetBool();
			}
			if (document.HasMember("characterParamHeightRatio"))
			{
				g_character_param_height_ratio = document["characterParamHeightRatio"].GetFloat();
			}
			if (document.HasMember("characterParamBustRatio"))
			{
				g_character_param_bust_ratio = document["characterParamBustRatio"].GetFloat();
			}
			if (document.HasMember("characterParamHeadRatio"))
			{
				g_character_param_head_ratio = document["characterParamHeadRatio"].GetFloat();
			}
			if (document.HasMember("characterParamArmRatio"))
			{
				g_character_param_arm_ratio = document["characterParamArmRatio"].GetFloat();
			}
			if (document.HasMember("characterParamHandRatio"))
			{
				g_character_param_hand_ratio = document["characterParamHandRatio"].GetFloat();
			}
			if (document.HasMember("replaceToCustomFont"))
			{
				g_replace_to_custom_font = document["replaceToCustomFont"].GetBool();
			}
			if (document.HasMember("fontAssetBundlePath"))
			{
				g_font_assetbundle_path = std::string(document["fontAssetBundlePath"].GetString());
			}
			if (document.HasMember("fontAssetName"))
			{
				g_font_asset_name = std::string(document["fontAssetName"].GetString());
			}
			if (document.HasMember("tmproFontAssetName"))
			{
				g_tmpro_font_asset_name = std::string(document["tmproFontAssetName"].GetString());
			}
			if (document.HasMember("antiAliasing"))
			{
				g_anti_aliasing = document["antiAliasing"].GetInt();
				std::vector<int> options = { 0, 2, 4, 8, -1 };
				g_anti_aliasing = std::find(options.begin(), options.end(), g_anti_aliasing) - options.begin();
			}

			if (document.HasMember("customTitleName"))
			{
				g_custom_title_name = document["customTitleName"].GetString();
			}

			if (document.HasMember("replaceAssetsPath"))
			{
				auto replaceAssetsPath = local::u8_wide(document["replaceAssetsPath"].GetString());
				if (PathIsRelativeW(replaceAssetsPath.data()))
				{
					replaceAssetsPath.insert(0, ((std::wstring)std::filesystem::current_path().native()).append(L"/"));
				}
				if (std::filesystem::exists(replaceAssetsPath) && std::filesystem::is_directory(replaceAssetsPath))
				{
					for (auto& file : std::filesystem::directory_iterator(replaceAssetsPath))
					{
						if (file.is_regular_file())
						{
							g_replace_assets.emplace(local::wide_u8(local::acp_wide(file.path().filename().string())), ReplaceAsset{ local::wide_u8(local::acp_wide(file.path().string())), nullptr });
						}
					}
				}
			}

			if (document.HasMember("replaceAssetBundleFilePath"))
			{
				g_replace_assetbundle_file_path = document["replaceAssetBundleFilePath"].GetString();
			}

			if (document.HasMember("replaceAssetBundleFilePaths") &&
				document["replaceAssetBundleFilePaths"].IsArray())
			{
				auto array = document["replaceAssetBundleFilePaths"].GetArray();
				for (auto it = array.Begin(); it != array.End(); it++)
				{
					auto value = it->GetString();
					g_replace_assetbundle_file_paths.emplace_back(value);
				}
			}

			if (document.HasMember("localizationJsonPath"))
			{
				auto path = document["localizationJsonPath"].GetString();

				std::ifstream localization_json_stream{ path };

				if (localization_json_stream.is_open())
				{
					rapidjson::IStreamWrapper wrapper{ localization_json_stream };
					localization_dict.ParseStream(wrapper);

					localization_json_stream.close();
				}
			}

			if (document.HasMember("dicts"))
			{
				auto& dicts_arr = document["dicts"];
				auto len = dicts_arr.Size();

				for (size_t i = 0; i < len; ++i)
				{
					auto dict = dicts_arr[i].GetString();

					dicts.emplace_back(dict);
				}
			}
		}
		else
		{
			has_json_parse_error = true;
			std::stringstream str_stream;
			str_stream << "JSON parse error: " << GetParseError_En(document.GetParseError()) << " (" << document.GetErrorOffset() << ")";
			json_parse_error_msg = str_stream.str();
		}

		config_stream.close();
		return dicts;
	}
}

int __stdcall DllMain(HINSTANCE, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// the DMM Launcher set start path to system32 wtf????
		string module_name;
		module_name.resize(MAX_PATH);
		module_name.resize(GetModuleFileName(nullptr, module_name.data(), MAX_PATH));

		filesystem::path module_path(local::wide_u8(local::acp_wide(module_name)));

		// check name
		if (module_path.filename() != "imasscprism.exe")
			return 1;

		std::filesystem::current_path(
			module_path.parent_path()
		);

		auto dicts = read_config();

		if (g_enable_console)
			create_debug_console();

		Game::CurrentGameRegion = Game::Region::JAP;

		init_hook_base();

		std::thread init_thread([dicts]() {
			logger::init_logger();
			local::load_textdb(&dicts);

			init_hook();

			if (g_enable_console)
				start_console();
			});
		init_thread.detach();
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		uninit_hook();
		logger::close_logger();
	}

	return 1;
}
