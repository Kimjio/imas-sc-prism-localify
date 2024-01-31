#include <stdinclude.hpp>

#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>

#include <array>

#include <set>
#include <sstream>
#include <string_view>

#include <Tlhelp32.h>

#include <regex>

#include <psapi.h>

#include <winhttp.h>

#include <wrl.h>
#include <wil/com.h>

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>

#include <SQLiteCpp/SQLiteCpp.h>

#include "ntdll.h"

#include "il2cpp_dump.h"

#include "config.hpp"

using namespace std;

using namespace Microsoft::WRL;

namespace
{
	void path_game_assembly();

	void patch_after_criware();

	bool mh_inited = false;

	void dump_bytes(void* pos)
	{
		if (pos)
		{
			printf("Hex dump of %p\n", pos);

			char* memory = reinterpret_cast<char*>(pos);

			for (int i = 0; i < 0x20; i++)
			{
				if (i > 0 && i % 16 == 0)
					printf("\n");

				char byte = *(memory++);

				printf("%02hhX ", byte);
			}

		}
		printf("\n\n");
	}

	void* Application_Quit_orig;

	void Application_Quit_hook(int exitCode)
	{
		reinterpret_cast<decltype(Application_Quit_hook)*>(Application_Quit_orig)(exitCode);
	}

	void* DMMGameGuard_Setup_orig = nullptr;

	void DMMGameGuard_Setup_hook(Il2CppObject* _this)
	{
		// no-op
	}

	void* PrismInitializer__initializeRuntime_orig = nullptr;

	void PrismInitializer__initializeRuntime_hook()
	{
		reinterpret_cast<decltype(PrismInitializer__initializeRuntime_hook)*>(PrismInitializer__initializeRuntime_orig)();

		auto hWnd = FindWindowW(L"UnityWndClass", L"imasscprism");
		if (hWnd)
		{
			if (!g_custom_title_name.empty())
			{
				SetWindowTextA(hWnd, local::wide_acp(local::u8_wide(g_custom_title_name)).data());
			}
			if (has_json_parse_error)
			{
				MessageBox(hWnd, json_parse_error_msg.data(), "Song for Prism Localify", MB_OK | MB_ICONWARNING);
			}
		}

		patch_after_criware();
	}

	void* il2cpp_init_orig = nullptr;
	bool __stdcall il2cpp_init_hook(const char* domain_name)
	{
		const auto result = reinterpret_cast<decltype(il2cpp_init_hook)*>(il2cpp_init_orig)(domain_name);
		il2cpp_symbols::init(GetModuleHandle("GameAssembly.dll"));

		auto Application_Quit = il2cpp_resolve_icall("UnityEngine.Application::Quit(System.Int32)");
		MH_CreateHook(Application_Quit, Application_Quit_hook, &Application_Quit_orig);
		MH_EnableHook(Application_Quit);

		auto DMMGameGuard_Setup = il2cpp_symbols::get_method_pointer("PRISM.Legacy.dll", "PRISM", "DMMGameGuard", "Setup", 0);
		MH_CreateHook(DMMGameGuard_Setup, DMMGameGuard_Setup_hook, &DMMGameGuard_Setup_orig);
		MH_EnableHook(DMMGameGuard_Setup);

		auto PrismInitializer__initializeRuntime = il2cpp_symbols::get_method_pointer("PRISM.Direction.dll", "PRISM.Direction", "PrismInitializer", "_initializeRuntime", 0);
		MH_CreateHook(PrismInitializer__initializeRuntime, PrismInitializer__initializeRuntime_hook, &PrismInitializer__initializeRuntime_orig);
		MH_EnableHook(PrismInitializer__initializeRuntime);

		path_game_assembly();

		return result;
	}

	HWND GetHWND()
	{
		auto title = local::u8_wide(g_custom_title_name);
		if (title.empty())
		{
			title = L"imasscprism";
		}

		auto hWnd = FindWindowA("UnityWndClass", local::wide_acp(title).data());

		if (!hWnd)
		{
			hWnd = FindWindowA("UnityWndClass", local::wide_acp(L"imasscprism").data());
		}

		return hWnd;
	}

	HINSTANCE hInstance;

	void* UnityMain_orig = nullptr;

	int __stdcall UnityMain_hook(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
	{
		::hInstance = hInstance;
		return reinterpret_cast<decltype(UnityMain_hook)*>(UnityMain_orig)(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
	}

	void* load_library_w_orig = nullptr;
	HMODULE WINAPI load_library_w_hook(LPCWSTR lpLibFileName)
	{
		if (lpLibFileName == L"GameAssembly.dll"s)
		{
			const auto il2cpp = reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(lpLibFileName);
			const auto il2cpp_init_addr = GetProcAddress(il2cpp, "il2cpp_init");
			MH_CreateHook(il2cpp_init_addr, il2cpp_init_hook, &il2cpp_init_orig);
			MH_EnableHook(il2cpp_init_addr);

			MH_DisableHook(LoadLibraryW);
			MH_RemoveHook(LoadLibraryW);

			return il2cpp;
		}

		return reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(lpLibFileName);
	}

	Il2CppObject* fontAssets = nullptr;

	vector<Il2CppObject*> replaceAssets;

	vector<string> replaceAssetNames;

	Il2CppObject* (*load_from_file)(Il2CppString* path, uint32_t crc, uint64_t offset);

	Il2CppObject* (*load_assets)(Il2CppObject* _this, Il2CppString* name, Il2CppObject* runtimeType);

	Il2CppArraySize* (*get_all_asset_names)(Il2CppObject* _this);

	Il2CppString* (*uobject_get_name)(Il2CppObject* uObject);

	Il2CppString* (*uobject_set_name)(Il2CppObject* uObject, Il2CppString* name);

	Il2CppString* (*get_unityVersion)();

	void PrintStackTrace()
	{
		Il2CppString* (*trace)() = il2cpp_symbols::get_method_pointer<Il2CppString * (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "StackTraceUtility", "ExtractStackTrace", 0);
		cout << local::wide_u8(wstring(trace()->start_char)) << "\n";
	}

	Il2CppObject* GetRuntimeType(const char* assemblyName, const char* namespaze, const char* klassName)
	{
		return il2cpp_type_get_object(il2cpp_class_get_type(il2cpp_symbols::get_class(assemblyName, namespaze, klassName)));
	}

	template<typename... T>
	Il2CppDelegate* CreateDelegateWithClass(Il2CppClass* klass, Il2CppObject* target, void (*fn)(Il2CppObject*, T...))
	{
		auto delegate = reinterpret_cast<MulticastDelegate*>(il2cpp_object_new(klass));
		delegate->method_ptr = reinterpret_cast<Il2CppMethodPointer>(fn);

		auto methodInfo = reinterpret_cast<MethodInfo*>(il2cpp_object_new(
			il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo")));
		methodInfo->methodPointer = delegate->method_ptr;
		methodInfo->klass = il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo");
		methodInfo->invoker_method = *([](Il2CppMethodPointer fn, const MethodInfo* method, void* obj, void** params)
			{
				return reinterpret_cast<void* (*)(void*, void**)>(fn)(obj, params);
			});
		delegate->method = methodInfo;

		auto object = reinterpret_cast<Il2CppObject*>(delegate);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// auto targetDest = reinterpret_cast<char*>(object) + targetField->offset;

		il2cpp_gc_wbarrier_set_field(object, reinterpret_cast<void**>(&(delegate)->target), target);

		// auto methodInfoField = il2cpp_class_get_field_from_name(object->klass, "method_info");
		// il2cpp_field_set_value(object, methodInfoField, methodInfo);

		// auto pointerField = il2cpp_class_get_field_from_name(object->klass, "method_ptr");
		// il2cpp_field_set_value(object, pointerField, fn);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// il2cpp_field_set_value(object, targetField, target);

		return delegate;
	}

	template<typename... T>
	Il2CppDelegate* CreateDelegate(Il2CppObject* target, void (*fn)(Il2CppObject*, T...))
	{
		auto delegate = reinterpret_cast<MulticastDelegate*>(
			il2cpp_object_new(il2cpp_symbols::get_class("mscorlib.dll", "System", "MulticastDelegate")));
		delegate->method_ptr = reinterpret_cast<Il2CppMethodPointer>(fn);

		auto methodInfo = reinterpret_cast<MethodInfo*>(il2cpp_object_new(
			il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo")));
		methodInfo->methodPointer = delegate->method_ptr;
		methodInfo->klass = il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo");
		methodInfo->invoker_method = *([](Il2CppMethodPointer fn, const MethodInfo* method, void* obj, void** params)
			{
				return reinterpret_cast<void* (*)(void*, void**)>(fn)(obj, params);
			});
		delegate->method = methodInfo;

		auto object = reinterpret_cast<Il2CppObject*>(delegate);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// auto targetDest = reinterpret_cast<char*>(object) + targetField->offset;

		il2cpp_gc_wbarrier_set_field(object, reinterpret_cast<void**>(&(delegate)->target), target);

		// auto methodInfoField = il2cpp_class_get_field_from_name(object->klass, "method_info");
		// il2cpp_field_set_value(object, methodInfoField, methodInfo);

		// auto pointerField = il2cpp_class_get_field_from_name(object->klass, "method_ptr");
		// il2cpp_field_set_value(object, pointerField, fn);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// il2cpp_field_set_value(object, targetField, target);

		return delegate;
	}

	template<typename... T>
	Il2CppDelegate* CreateUnityAction(Il2CppObject* target, void (*fn)(Il2CppObject*, T...))
	{
		auto delegate = reinterpret_cast<MulticastDelegate*>(
			il2cpp_object_new(il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine.Events", "UnityAction")));
		delegate->method_ptr = reinterpret_cast<Il2CppMethodPointer>(fn);

		auto methodInfo = reinterpret_cast<MethodInfo*>(il2cpp_object_new(
			il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo")));
		methodInfo->methodPointer = delegate->method_ptr;
		methodInfo->klass = il2cpp_symbols::get_class("mscorlib.dll", "System.Reflection", "MethodInfo");
		methodInfo->invoker_method = *([](Il2CppMethodPointer fn, const MethodInfo* method, void* obj, void** params)
			{
				return reinterpret_cast<void* (*)(void*, void**)>(fn)(obj, params);
			});
		delegate->method = methodInfo;

		auto object = reinterpret_cast<Il2CppObject*>(delegate);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// auto targetDest = reinterpret_cast<char*>(object) + targetField->offset;

		il2cpp_gc_wbarrier_set_field(object, reinterpret_cast<void**>(&(delegate)->target), target);

		// auto methodInfoField = il2cpp_class_get_field_from_name(object->klass, "method_info");
		// il2cpp_field_set_value(object, methodInfoField, methodInfo);

		// auto pointerField = il2cpp_class_get_field_from_name(object->klass, "method_ptr");
		// il2cpp_field_set_value(object, pointerField, fn);

		// auto targetField = il2cpp_class_get_field_from_name(object->klass, "m_target");
		// il2cpp_field_set_value(object, targetField, target);

		return delegate;
	}

	Il2CppObject* GetSingletonInstance(Il2CppClass* klass)
	{
		if (!klass || !klass->parent)
		{
			return nullptr;
		}

		auto instanceField = il2cpp_class_get_field_from_name(klass, "instance");
		if (!instanceField)
		{
			auto get_Instance = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)()>(klass, "get_Instance", -1);
			if (get_Instance)
			{
				return get_Instance->methodPointer();
			}

			return nullptr;
		}

		Il2CppObject* instance;
		il2cpp_field_static_get_value(instanceField, &instance);

		if (instance)
		{
			return instance;
		}

		auto get_Instance = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)()>(klass, "get_Instance", -1);
		if (get_Instance)
		{
			return get_Instance->methodPointer();
		}

		return nullptr;
	}

	Boolean GetBoolean(bool value)
	{
		return il2cpp_symbols::get_method_pointer<Boolean(*)(Il2CppString * value)>(
			"mscorlib.dll", "System", "Boolean", "Parse", 1)(
				il2cpp_string_new(value ? "true" : "false"));
	}

	Int32Object* GetInt32Instance(int value)
	{
		auto int32Class = il2cpp_symbols::get_class("mscorlib.dll", "System", "Int32");
		auto instance = il2cpp_object_new(int32Class);
		il2cpp_runtime_object_init(instance);
		auto m_value = il2cpp_class_get_field_from_name(int32Class, "m_value");
		il2cpp_field_set_value(instance, m_value, &value);
		return (Int32Object*)instance;
	}

	Il2CppObject* ParseEnum(Il2CppObject* runtimeType, const string& name)
	{
		return il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppObject*, Il2CppString*)>("mscorlib.dll", "System", "Enum", "Parse", 2)(runtimeType, il2cpp_string_new(name.data()));
	}

	Il2CppString* GetEnumName(Il2CppObject* runtimeType, int id)
	{
		return il2cpp_symbols::get_method_pointer<Il2CppString * (*)(Il2CppObject*, Int32Object*)>("mscorlib.dll", "System", "Enum", "GetName", 2)(runtimeType, GetInt32Instance(id));
	}

	unsigned long GetEnumValue(Il2CppObject* runtimeEnum)
	{
		return il2cpp_symbols::get_method_pointer<unsigned long (*)(Il2CppObject*)>("mscorlib.dll", "System", "Enum", "ToUInt64", 1)(runtimeEnum);
	}

	Il2CppObject* GetCustomFont()
	{
		if (!fontAssets) return nullptr;
		if (!g_font_asset_name.empty())
		{
			return load_assets(fontAssets, il2cpp_string_new(g_font_asset_name.data()), GetRuntimeType("UnityEngine.TextRenderingModule.dll", "UnityEngine", "Font"));
		}
		return nullptr;
	}

	// Fallback not support outline style
	Il2CppObject* GetCustomTMPFontFallback()
	{
		if (!fontAssets) return nullptr;
		auto font = GetCustomFont();
		if (font)
		{
			return il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(
				Il2CppObject * font, int samplingPointSize, int atlasPadding, int renderMode, int atlasWidth, int atlasHeight, int atlasPopulationMode, bool enableMultiAtlasSupport
				)>("Unity.TextMeshPro.dll", "TMPro", "TMP_FontAsset", "CreateFontAsset", 1)
				(font, 36, 4, 4165, 8192, 8192, 1, false);
		}
		return nullptr;
	}

	Il2CppObject* GetCustomTMPFont()
	{
		if (!fontAssets) return nullptr;
		if (!g_tmpro_font_asset_name.empty())
		{
			auto tmpFont = load_assets(fontAssets, il2cpp_string_new(g_tmpro_font_asset_name.data()), GetRuntimeType("Unity.TextMeshPro.dll", "TMPro", "TMP_FontAsset"));
			return tmpFont ? tmpFont : GetCustomTMPFontFallback();
		}
		return GetCustomTMPFontFallback();
	}

	void* assetbundle_load_asset_orig = nullptr;
	Il2CppObject* assetbundle_load_asset_hook(Il2CppObject* _this, Il2CppString* name, const Il2CppType* type);

	Il2CppObject* GetReplacementAssets(Il2CppString* name, const Il2CppType* type)
	{
		for (auto it = replaceAssets.begin(); it != replaceAssets.end(); it++)
		{
			auto assets = reinterpret_cast<decltype(assetbundle_load_asset_hook)*>(assetbundle_load_asset_orig)(*it, name, type);
			if (assets)
			{
				return assets;
			}
		}
		return nullptr;
	}

	string GetUnityVersion()
	{
		string version(local::wide_u8(get_unityVersion()->start_char));
		return version;
	}

	void* populate_with_errors_orig = nullptr;
	bool populate_with_errors_hook(Il2CppObject* _this, Il2CppString* str, TextGenerationSettings_t* settings, void* context)
	{
		return reinterpret_cast<decltype(populate_with_errors_hook)*>(populate_with_errors_orig) (
			_this, local::get_localized_string(str), settings, context
			);
	}

	void* get_preferred_width_orig = nullptr;
	float get_preferred_width_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings)
	{
		return reinterpret_cast<decltype(get_preferred_width_hook)*>(get_preferred_width_orig) (
			_this, local::get_localized_string(str), settings
			);
	}

	bool useDefaultFPS = false;

	void* set_fps_orig = nullptr;
	void set_fps_hook(int value)
	{
		reinterpret_cast<decltype(set_fps_hook)*>(set_fps_orig)(useDefaultFPS ? value : g_max_fps);
	}

	Il2CppObject* (*display_get_main)();

	int (*get_system_width)(Il2CppObject* _this);

	int (*get_system_height)(Il2CppObject* _this);

	int (*get_rendering_width)(Il2CppObject* _this);

	int (*get_rendering_height)(Il2CppObject* _this);

	const float ratio_16_9 = 1.778f;
	const float ratio_9_16 = 0.563f;

	float ratio_vertical = 0.5625f;
	float ratio_horizontal = 1.7777778f;

	void (*get_resolution)(Resolution_t* buffer);

	void (*set_scale_factor)(void*, float);

	void* canvas_scaler_setres_orig;
	void canvas_scaler_setres_hook(Il2CppObject* _this, Vector2_t res)
	{
		int width = il2cpp_symbols::get_method_pointer<int (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "Screen", "get_width", -1)();
		int height = il2cpp_symbols::get_method_pointer<int (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "Screen", "get_height", -1)();

		res.x = width;
		res.y = height;

		// set scale factor to make ui bigger on hi-res screen
		if (width < height)
		{
			float scale = min(g_ui_scale, max(1, height * ratio_vertical) * g_ui_scale);
			set_scale_factor(_this, scale);
		}
		else
		{
			float scale = min(g_ui_scale, max(1, width / ratio_horizontal) * g_ui_scale);
			set_scale_factor(_this, scale);
		}

		reinterpret_cast<decltype(canvas_scaler_setres_hook)*>(canvas_scaler_setres_orig)(_this, res);
	}

	void* set_resolution_orig;
	void set_resolution_hook(int width, int height, int fullscreenMode, RefreshRate_t perferredRefreshRate)
	{
		return;
	}

	WNDPROC oldWndProcPtr = nullptr;

	void* wndproc_orig = nullptr;
	LRESULT wndproc_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_XBUTTONDOWN && GET_KEYSTATE_WPARAM(wParam) == MK_XBUTTON1)
		{
			array<INPUT, 2> inputs = {
				INPUT
				{
					INPUT_KEYBOARD,
					.ki = KEYBDINPUT{ VK_ESCAPE }
				},
				INPUT
				{
					INPUT_KEYBOARD,
					.ki = KEYBDINPUT{ VK_ESCAPE, .dwFlags = KEYEVENTF_KEYUP }
				}
			};

			SendInput(inputs.size(), inputs.data(), sizeof(INPUT));
			return TRUE;
		}

		if (uMsg == WM_SYSKEYDOWN)
		{
			bool altDown = (lParam & (static_cast<long long>(1) << 29)) != 0;

			if (g_max_fps > -1 && wParam == 'F' && altDown)
			{
				useDefaultFPS = !useDefaultFPS;
				set_fps_hook(30);
				return TRUE;
			}

			if (wParam == VK_RETURN && altDown)
			{
				if (oldWndProcPtr)
				{
					CallWindowProcW(oldWndProcPtr, hWnd, uMsg, wParam, lParam);

					auto isFullscreen = il2cpp_symbols::get_method_pointer<bool (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "Screen", "get_fullScreen", -1)();

					if (isFullscreen)
					{
						thread([hWnd]()
							{
								Sleep(100);

								long style = GetWindowLongW(hWnd, GWL_STYLE);
								style |= WS_MAXIMIZEBOX;
								style |= WS_THICKFRAME;
								SetWindowLongPtrW(hWnd, GWL_STYLE, style);
							}
						).detach();
					}
				}
				return TRUE;
			}
		}

		if (uMsg == WM_SIZE)
		{
			// auto renderManager = GetSingletonInstance(il2cpp_symbols::get_class("Prism.Rendering.Runtime.dll", "PRISM.Rendering", "RenderManager"));

			//if (renderManager)
			//{
			//	RECT windowRect;
			//	GetWindowRect(hWnd, &windowRect);
			//	int windowWidth = windowRect.right - windowRect.left;
			//	int windowHeight = windowRect.bottom - windowRect.top;

			//	auto widthField = il2cpp_class_get_field_from_name(renderManager->klass, "DefaultScreenWidth");
			//	auto heightField = il2cpp_class_get_field_from_name(renderManager->klass, "DefaultScreenHeight");

			//	// il2cpp_field_static_set_value(widthField, &windowWidth);
			//	// il2cpp_field_static_set_value(heightField, &windowHeight);
			//}
		}

		if (uMsg == WM_CLOSE)
		{
			ExitProcess(0);
			return TRUE;
		}

		if (oldWndProcPtr)
		{
			return CallWindowProcW(oldWndProcPtr, hWnd, uMsg, wParam, lParam);
		}

		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}

	void (*text_assign_font)(void*);
	void (*text_set_font)(void*, Il2CppObject*);
	Il2CppObject* (*text_get_font)(void*);
	int (*text_get_size)(void*);
	void (*text_set_size)(void*, int);
	float (*text_get_linespacing)(void*);
	void (*text_set_style)(void*, int);
	void (*text_set_linespacing)(void*, float);
	Il2CppString* (*text_get_text)(void*);
	void (*text_set_text)(void*, Il2CppString*);
	void (*text_set_horizontalOverflow)(void*, int);
	void (*text_set_verticalOverflow)(void*, int);

	void* on_populate_orig = nullptr;
	void on_populate_hook(Il2CppObject* _this, void* toFill)
	{
		if (g_replace_to_custom_font)
		{
			auto font = text_get_font(_this);
			Il2CppString* name = uobject_get_name(font);
			if (g_font_asset_name.find(local::wide_u8(name->start_char)) == string::npos)
			{
				text_set_font(_this, GetCustomFont());
			}
		}

		text_set_text(_this, local::get_localized_string(text_get_text(_this)));

		return reinterpret_cast<decltype(on_populate_hook)*>(on_populate_orig)(_this, toFill);
	}

	void* TMP_Text_GetTextElement_orig = nullptr;

	Il2CppObject* TMP_Text_GetTextElement_hook(Il2CppObject* _this, uint32_t unicode, Il2CppObject* fontAsset, int fontStyle, int fontWeight, bool* isUsingAlternativeTypeface)
	{
		auto enableVertexGradient = il2cpp_class_get_method_from_name_type<bool (*)(Il2CppObject*)>(_this->klass, "get_enableVertexGradient", 0)->methodPointer(_this);

		if ((enableVertexGradient && uobject_get_name(_this)->start_char == L"Name"s) || uobject_get_name(_this)->start_char == L"NickName"s)
		{
			return reinterpret_cast<decltype(TMP_Text_GetTextElement_hook)*>(TMP_Text_GetTextElement_orig)(_this, unicode, fontAsset, fontStyle, fontWeight, isUsingAlternativeTypeface);
		}

		static auto customFont = GetCustomTMPFont();

		return reinterpret_cast<decltype(TMP_Text_GetTextElement_hook)*>(TMP_Text_GetTextElement_orig)(_this, unicode, customFont, fontStyle, fontWeight, isUsingAlternativeTypeface);
	}

	void* UITextMeshProUGUI_set_text_orig = nullptr;
	void UITextMeshProUGUI_set_text_hook(Il2CppObject* _this, Il2CppString* value)
	{
		reinterpret_cast<decltype(UITextMeshProUGUI_set_text_hook)*>(UITextMeshProUGUI_set_text_orig)(_this, local::get_localized_string(value));
	}

	void* UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_orig = nullptr;
	Il2CppObject* UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_hook(Il2CppObject* _this)
	{
		auto obj = reinterpret_cast<decltype(UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_hook)*>(UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_orig)(_this);

		if (obj)
		{
			if (obj->klass->name == "String"s)
			{
				return reinterpret_cast<Il2CppObject*>(local::get_localized_string(reinterpret_cast<Il2CppString*>(obj)));
			}

			if (obj->klass->name == "Texture2D"s)
			{
				if (!local::wide_u8(uobject_get_name(obj)->start_char).empty())
				{
					auto newTexture = GetReplacementAssets(
						uobject_get_name(obj),
						(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
					if (newTexture)
					{
						il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
							(newTexture, 32);
						return newTexture;
					}
				}
				return obj;
			}
		}

		return obj;
	}

	void* UITextMeshProUGUI_Awake_orig = nullptr;
	void UITextMeshProUGUI_Awake_hook(Il2CppObject* _this)
	{
		UITextMeshProUGUI_set_text_hook(_this, reinterpret_cast<Il2CppString*>(
			reinterpret_cast<decltype(UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_hook)*>(UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_orig)(_this)));

		auto enableVertexGradient = il2cpp_class_get_method_from_name_type<bool (*)(Il2CppObject*)>(_this->klass, "get_enableVertexGradient", 0)->methodPointer(_this);

		if ((enableVertexGradient && uobject_get_name(_this)->start_char == L"Name"s) || uobject_get_name(_this)->start_char == L"NickName"s)
		{
			auto fontAssetField = il2cpp_class_get_field_from_name(_this->klass, "m_fontAsset");

			il2cpp_field_set_value(_this, fontAssetField, GetCustomTMPFont());
		}

		reinterpret_cast<decltype(UITextMeshProUGUI_Awake_hook)*>(UITextMeshProUGUI_Awake_orig)(_this);
	}

	void* RenderFrameRate_ChangeFrameRate_orig = nullptr;
	void RenderFrameRate_ChangeFrameRate_hook(int value)
	{
		reinterpret_cast<decltype(RenderFrameRate_ChangeFrameRate_hook)*>(RenderFrameRate_ChangeFrameRate_orig)(g_max_fps);
	}

	void* SystemUtility_set_FrameRate_orig = nullptr;
	void SystemUtility_set_FrameRate_hook(Il2CppObject* _this, int value)
	{
		reinterpret_cast<decltype(SystemUtility_set_FrameRate_hook)*>(SystemUtility_set_FrameRate_orig)(_this, g_max_fps);
	}

	void* RenderManager_SetResolutionRate_orig = nullptr;
	void RenderManager_SetResolutionRate_hook(Il2CppObject* _this, int type, float rate)
	{
		reinterpret_cast<decltype(RenderManager_SetResolutionRate_hook)*>(RenderManager_SetResolutionRate_orig)(_this, type, 1);
	}

	void* RenderManager_GetResolutionRate_orig = nullptr;
	float RenderManager_GetResolutionRate_hook(Il2CppObject* _this, int type)
	{
		return 1;
	}

	void* RenderManager_GetResolutionSize_orig = nullptr;
	Vector2Int_t RenderManager_GetResolutionSize_hook(Il2CppObject* _this, Il2CppObject* targetCamera, int rateType)
	{
		auto size = reinterpret_cast<decltype(RenderManager_GetResolutionSize_hook)*>(RenderManager_GetResolutionSize_orig)(_this, targetCamera, rateType);

		size.x *= g_resolution_3d_scale;
		size.y *= g_resolution_3d_scale;

		return size;
	}

	void* set_anti_aliasing_orig = nullptr;
	void set_anti_aliasing_hook(int level)
	{
		reinterpret_cast<decltype(set_anti_aliasing_hook)*>(set_anti_aliasing_orig)(g_anti_aliasing);
	}

	void* set_shadowResolution_orig = nullptr;
	void set_shadowResolution_hook(int level)
	{
		reinterpret_cast<decltype(set_shadowResolution_hook)*>(set_shadowResolution_orig)(3);
	}

	void* set_anisotropicFiltering_orig = nullptr;
	void set_anisotropicFiltering_hook(int mode)
	{
		reinterpret_cast<decltype(set_anisotropicFiltering_hook)*>(set_anisotropicFiltering_orig)(2);
	}

	void* set_shadows_orig = nullptr;
	void set_shadows_hook(int quality)
	{
		reinterpret_cast<decltype(set_shadows_hook)*>(set_shadows_orig)(2);
	}

	void* set_softVegetation_orig = nullptr;
	void set_softVegetation_hook(bool enable)
	{
		reinterpret_cast<decltype(set_softVegetation_hook)*>(set_softVegetation_orig)(true);
	}

	void* set_realtimeReflectionProbes_orig = nullptr;
	void set_realtimeReflectionProbes_hook(bool enable)
	{
		reinterpret_cast<decltype(set_realtimeReflectionProbes_hook)*>(set_realtimeReflectionProbes_orig)(true);
	}

	void* Light_set_shadowResolution_orig = nullptr;
	void Light_set_shadowResolution_hook(Il2CppObject* _this, int level)
	{
		reinterpret_cast<decltype(Light_set_shadowResolution_hook)*>(Light_set_shadowResolution_orig)(_this, 3);
	}

	void* PathResolver_GetLocalPath_orig = nullptr;
	Il2CppString* PathResolver_GetLocalPath_hook(Il2CppObject* _this, int kind, Il2CppString* hname)
	{
		auto hnameU8 = local::wide_u8(hname->start_char);
		if (g_replace_assets.find(hnameU8) != g_replace_assets.end())
		{
			auto& replaceAsset = g_replace_assets.at(hnameU8);
			return il2cpp_string_new(replaceAsset.path.data());
		}
		return reinterpret_cast<decltype(PathResolver_GetLocalPath_hook)*>(PathResolver_GetLocalPath_orig)(_this, kind, hname);
	}

	Il2CppObject* Renderer_get_material_hook(Il2CppObject* _this);
	Il2CppArraySize* Renderer_get_materials_hook(Il2CppObject* _this);
	Il2CppObject* Renderer_get_sharedMaterial_hook(Il2CppObject* _this);
	Il2CppArraySize* Renderer_get_sharedMaterials_hook(Il2CppObject* _this);

	int (*Shader_PropertyToID)(Il2CppString* name);

	Il2CppObject* Material_GetTextureImpl_hook(Il2CppObject* _this, int nameID);
	void Material_SetTextureImpl_hook(Il2CppObject* _this, int nameID, Il2CppObject* texture);
	bool (*Material_HasProperty)(Il2CppObject* _this, int nameID);

	bool (*uobject_IsNativeObjectAlive)(Il2CppObject* uObject);

	void ReplaceRendererTexture(Il2CppObject* renderer)
	{
		if (!uobject_IsNativeObjectAlive(renderer) || true)
		{
			return;
		}
		Renderer_get_materials_hook(renderer);
		Renderer_get_material_hook(renderer);
		Renderer_get_sharedMaterials_hook(renderer);
		Renderer_get_sharedMaterial_hook(renderer);
	}

	void ReplaceMaterialTexture(Il2CppObject* material)
	{
		if (!uobject_IsNativeObjectAlive(material))
		{
			return;
		}
		if (Material_HasProperty(material, Shader_PropertyToID(il2cpp_string_new("_MainTex"))))
		{
			auto mainTexture = Material_GetTextureImpl_hook(material, Shader_PropertyToID(il2cpp_string_new("_MainTex")));
			if (mainTexture)
			{
				auto uobject_name = uobject_get_name(mainTexture);
				if (!local::wide_u8(uobject_name->start_char).empty())
				{
					auto newTexture = GetReplacementAssets(
						uobject_name,
						(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
					if (newTexture)
					{
						il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
							(newTexture, 32);
						Material_SetTextureImpl_hook(material, Shader_PropertyToID(il2cpp_string_new("_MainTex")), newTexture);
					}
				}
			}
		}
	}

	void ReplaceAssetHolderTextures(Il2CppObject* holder)
	{
		if (!uobject_IsNativeObjectAlive(holder))
		{
			return;
		}
		auto objectList = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(holder->klass, "get_ObjectList", 0)->methodPointer(holder);
		FieldInfo* itemsField = il2cpp_class_get_field_from_name(objectList->klass, "_items");
		Il2CppArraySize* arr;
		il2cpp_field_get_value(objectList, itemsField, &arr);
		for (int i = 0; i < arr->max_length; i++)
		{
			auto pair = (Il2CppObject*)arr->vector[i];
			if (!pair) continue;
			auto field = il2cpp_class_get_field_from_name(pair->klass, "Value");
			Il2CppObject* obj;
			il2cpp_field_get_value(pair, field, &obj);
			if (obj)
			{
				//cout << "AssetHolder: " << i << " " << obj->klass->name << endl;
				if (obj->klass->name == "GameObject"s && uobject_IsNativeObjectAlive(obj))
				{
					// auto getComponent = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*, Il2CppType*)>(component->klass, "GetComponent", 1)->methodPointer;
					auto getComponents = il2cpp_class_get_method_from_name_type<Il2CppArraySize * (*)(Il2CppObject*, Il2CppType*, bool, bool, bool, bool, Il2CppObject*)>(obj->klass, "GetComponentsInternal", 6)->methodPointer;

					auto array = getComponents(obj, reinterpret_cast<Il2CppType*>(GetRuntimeType(
						"UnityEngine.CoreModule.dll", "UnityEngine", "Object")), true, true, true, false, nullptr);

					if (array)
					{
						for (int j = 0; j < array->max_length; j++)
						{
							auto obj =
								il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppObject*, long index)>("mscorlib.dll", "System", "Array", "GetValue", 1)(&array->obj, j);
							if (!obj) continue;
							/*if (obj && obj->klass && obj->klass->name != "Transform"s)
							{
								cout << obj->klass->name << endl;
							}*/
							if (string(obj->klass->name).find("MeshRenderer") != string::npos)
							{
								ReplaceRendererTexture(obj);
							}
						}
					}
				}
				if (obj->klass->name == "Texture2D"s)
				{
					auto uobject_name = uobject_get_name(obj);
					//cout << "Texture2D: " << local::wide_u8(uobject_name->start_char) << endl;
					if (!local::wide_u8(uobject_name->start_char).empty())
					{
						auto newTexture = GetReplacementAssets(
							uobject_name,
							(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
						if (newTexture)
						{
							il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
								(newTexture, 32);
							il2cpp_field_set_value(pair, field, newTexture);
						}
					}
				}
				if (obj->klass->name == "Material"s)
				{
					ReplaceMaterialTexture(obj);
				}
			}
		}
	}

	void ReplaceRawImageTexture(Il2CppObject* rawImage)
	{
		if (!uobject_IsNativeObjectAlive(rawImage))
		{
			return;
		}
		auto textureField = il2cpp_class_get_field_from_name(rawImage->klass, "m_Texture");
		Il2CppObject* texture;
		il2cpp_field_get_value(rawImage, textureField, &texture);
		if (texture)
		{
			auto uobject_name = uobject_get_name(texture);
			if (uobject_name)
			{
				auto nameU8 = local::wide_u8(uobject_name->start_char);
				if (!nameU8.empty())
				{
					do
					{
						stringstream pathStream(nameU8);
						string segment;
						vector<string> split;
						while (getline(pathStream, segment, '/'))
						{
							split.emplace_back(segment);
						}
						auto& textureName = split.back();
						if (!textureName.empty())
						{
							auto texture2D = GetReplacementAssets(il2cpp_string_new(split.back().data()),
								reinterpret_cast<Il2CppType*>(GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D")));
							if (texture2D)
							{
								il2cpp_field_set_value(rawImage, textureField, texture2D);
							}
						}
					} while (false);
				}
			}
		}
	}

	void* assetbundle_LoadFromFile_orig = nullptr;
	Il2CppObject* assetbundle_LoadFromFile_hook(Il2CppString* path, uint32_t crc, uint64_t offset)
	{
		stringstream pathStream(local::wide_u8(path->start_char));
		string segment;
		vector<string> splited;
		while (getline(pathStream, segment, '\\'))
		{
			splited.emplace_back(segment);
		}

		if (g_replace_assets.find(splited.back()) != g_replace_assets.end())
		{
			auto& replaceAsset = g_replace_assets.at(splited.back());
			auto assets = reinterpret_cast<decltype(assetbundle_LoadFromFile_hook)*>(assetbundle_LoadFromFile_orig)(il2cpp_string_new(replaceAsset.path.data()), crc, 0);
			replaceAsset.asset = assets;
			return assets;
		}

		auto bundle = reinterpret_cast<decltype(assetbundle_LoadFromFile_hook)*>(assetbundle_LoadFromFile_orig)(path, crc, offset);
		return bundle;
	}

	void DumpTexture2D(Il2CppObject* texture, string type)
	{
		auto textureName = local::wide_u8(uobject_get_name(texture)->start_char);

		auto width = il2cpp_class_get_method_from_name_type<int (*)(Il2CppObject*)>(texture->klass, "get_width", 0)->methodPointer(texture);

		auto height = il2cpp_class_get_method_from_name_type<int (*)(Il2CppObject*)>(texture->klass, "get_height", 0)->methodPointer(texture);

		auto renderTexture = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(int, int, int, int, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "GetTemporary", 5)(width, height, 0, 0, 1);

		il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "Graphics", "Blit", 2)(texture, renderTexture);

		auto previous = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "get_active", -1)();

		il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "set_active", 1)(renderTexture);

		auto readableTexture = il2cpp_object_new(il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
		il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*, int, int)>(readableTexture->klass, ".ctor", 2)->methodPointer(readableTexture, width, height);

		il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*, Rect_t, int, int)>(readableTexture->klass, "ReadPixels", 3)->methodPointer(readableTexture, Rect_t{ 0, 0, static_cast<float>(width), static_cast<float>(height) }, 0, 0);
		il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*)>(readableTexture->klass, "Apply", 0)->methodPointer(readableTexture);

		il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "set_active", 1)(previous);

		il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "ReleaseTemporary", 1)(renderTexture);

		auto method = il2cpp_symbols::get_method("UnityEngine.ImageConversionModule.dll", "UnityEngine", "ImageConversion", "EncodeToPNG", 1);

		void** params = new void* [1];
		params[0] = readableTexture;

		Il2CppException* exception;

		auto pngData = reinterpret_cast<Il2CppArraySize_t<uint8_t>*>(il2cpp_runtime_invoke(method, nullptr, params, &exception));

		if (exception)
		{
			il2cpp_raise_exception(exception);
			return;
		}

		replaceAll(textureName, "|", "_");

		if (!filesystem::exists("TextureDump"))
		{
			filesystem::create_directory("TextureDump");
		}

		if (!filesystem::exists("TextureDump/" + type))
		{
			filesystem::create_directory("TextureDump/" + type);
		}

		if (textureName.find(".png") == string::npos)
		{
			il2cpp_symbols::get_method_pointer<void (*)(Il2CppString*, Il2CppArraySize_t<uint8_t>*)>("mscorlib.dll", "System.IO", "File", "WriteAllBytes", 2)(il2cpp_string_new(("TextureDump/" + type + "/" + textureName + ".png").data()), pngData);
		}
		else
		{
			il2cpp_symbols::get_method_pointer<void (*)(Il2CppString*, Il2CppArraySize_t<uint8_t>*)>("mscorlib.dll", "System.IO", "File", "WriteAllBytes", 2)(il2cpp_string_new(("TextureDump/" + type + "/" + textureName).data()), pngData);
		}
	}

	void* Renderer_get_material_orig;
	Il2CppObject* Renderer_get_material_hook(Il2CppObject* _this);

	void* Material_GetTextureImpl_orig;
	Il2CppObject* Material_GetTextureImpl_hook(Il2CppObject* _this, int nameID);

	struct HowToPlayPopupResourceConfig_Item
	{
		Il2CppObject* Image;
		Il2CppString* Summary;
	};

	Il2CppObject* assetbundle_load_asset_hook(Il2CppObject* _this, Il2CppString* name, const Il2CppType* type)
	{
		stringstream pathStream(local::wide_u8(name->start_char));
		string segment;
		vector<string> splited;
		while (getline(pathStream, segment, '/'))
		{
			splited.emplace_back(segment);
		}

		auto& fileName = splited.back();
		if (find_if(replaceAssetNames.begin(), replaceAssetNames.end(), [fileName](const string& item)
			{
				return item.find(fileName) != string::npos;
			}) != replaceAssetNames.end())
		{
			return GetReplacementAssets(il2cpp_string_new(fileName.data()), type);
		}

		auto obj = reinterpret_cast<decltype(assetbundle_load_asset_hook)*>(assetbundle_load_asset_orig)(_this, name, type);

		if (g_dump_texture)
		{
			if (obj->klass->name == "GameObject"s)
			{
				auto getComponents = il2cpp_class_get_method_from_name_type<Il2CppArraySize * (*)(Il2CppObject*, Il2CppType*, bool, bool, bool, bool, Il2CppObject*)>(obj->klass, "GetComponentsInternal", 6)->methodPointer;

				auto array = getComponents(obj, reinterpret_cast<Il2CppType*>(GetRuntimeType(
					"UnityEngine.CoreModule.dll", "UnityEngine", "Object")), true, true, true, false, nullptr);

				if (array)
				{
					for (int j = 0; j < array->max_length; j++)
					{
						auto obj =
							il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppObject*, long index)>("mscorlib.dll", "System", "Array", "GetValue", 1)(&array->obj, j);
						if (!obj) continue;

						if (obj && obj->klass && obj->klass->name != "Transform"s)
						{
							if (obj->klass->name == "Image"s)
							{

								auto sprite = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_sprite", 0)->methodPointer(obj);

								if (sprite)
								{
									auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(sprite->klass, "get_texture", 0)->methodPointer(sprite);

									DumpTexture2D(texture, "Image");
								}
							}

							if (obj->klass->name == "RawImage"s)
							{
								auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_texture", 0)->methodPointer(obj);

								if (texture)
								{
									DumpTexture2D(texture, "RawImage");
								}
							}

							if (obj->klass->name == "MeshRenderer"s)
							{
								auto material = reinterpret_cast<decltype(Renderer_get_material_hook)*>(Renderer_get_material_orig)(obj);

								if (material)
								{
									auto mainTexture = reinterpret_cast<decltype(Material_GetTextureImpl_hook)*>(Material_GetTextureImpl_orig)(material, Shader_PropertyToID(il2cpp_string_new("_MainTex")));

									if (mainTexture)
									{
										DumpTexture2D(mainTexture, "MeshRenderer");
									}
								}
							}

							if (obj->klass->name == "HomeSpecialMissionBannerView"s)
							{
								auto bannerImageField = il2cpp_class_get_field_from_name(obj->klass, "bannerImage");
								Il2CppObject* bannerImage;
								il2cpp_field_get_value(obj, bannerImageField, &bannerImage);

								auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(bannerImage->klass, "get_texture", 0)->methodPointer(bannerImage);

								if (texture)
								{
									DumpTexture2D(texture, "RawImage");
								}
							}
						}
					}
				}
			}

			if (obj->klass->name == "Sprite"s)
			{
				auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_texture", 0)->methodPointer(obj);

				DumpTexture2D(texture, "Sprite");
			}

			if (obj->klass->name == "Image"s || obj->klass->name == "UIImage"s)
			{
				auto sprite = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_sprite", 0)->methodPointer(obj);

				auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(sprite->klass, "get_texture", 0)->methodPointer(sprite);

				DumpTexture2D(texture, "Image");
			}

			if (obj->klass->name == "Sprite"s)
			{
				auto texture = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(obj->klass, "get_texture", 0)->methodPointer(obj);

				DumpTexture2D(texture, "Sprite");
			}

			if (obj->klass->name == "Texture2D"s)
			{
				DumpTexture2D(obj, "Texture2D");
			}
		}

		if (obj->klass->name == "HowToPlayPopupResourceConfig"s)
		{
			auto itemsField = il2cpp_class_get_field_from_name(obj->klass, "items");
			Il2CppArraySize_t<HowToPlayPopupResourceConfig_Item>* items;
			il2cpp_field_get_value(obj, itemsField, &items);

			for (int i = 0; i < items->max_length; i++)
			{
				if (g_dump_texture)
				{
					DumpTexture2D(items->vector[i].Image, "Texture2D");
				}

				auto texture = items->vector[i].Image;

				auto uobject_name = uobject_get_name(texture);
				if (!local::wide_u8(uobject_name->start_char).empty())
				{
					auto newTexture = GetReplacementAssets(
						uobject_name,
						(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));

					if (newTexture)
					{
						il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
							(newTexture, 32);
						items->vector[i].Image = newTexture;
					}
				}
			}
		}

		return obj;
	}

	void* ImageConversion_LoadImage_orig = nullptr;
	bool ImageConversion_LoadImage_hook(Il2CppObject* tex, Il2CppArraySize_t<char>* data, bool markNonReadable)
	{
		auto hashData = hash<string_view>()(string_view(data->vector, data->max_length));
		auto fileName = to_string(hashData) + ".png";

		if (g_dump_texture)
		{
			if (!filesystem::exists("TextureDump"))
			{
				filesystem::create_directory("TextureDump");
			}

			if (!filesystem::exists("TextureDump/DownloadedTexture2D"))
			{
				filesystem::create_directory("TextureDump/DownloadedTexture2D");
			}

			il2cpp_symbols::get_method_pointer<void (*)(Il2CppString*, Il2CppArraySize_t<char>*)>("mscorlib.dll", "System.IO", "File", "WriteAllBytes", 2)(il2cpp_string_new(("TextureDump/DownloadedTexture2D/" + fileName).data()), data);
		}


		if (find_if(replaceAssetNames.begin(), replaceAssetNames.end(), [fileName](const string& item)
			{
				return item.find(fileName) != string::npos;
			}) != replaceAssetNames.end())
		{
			auto texture = GetReplacementAssets(il2cpp_string_new(fileName.data()), (Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));

			auto width = il2cpp_class_get_method_from_name_type<int (*)(Il2CppObject*)>(texture->klass, "get_width", 0)->methodPointer(texture);

			auto height = il2cpp_class_get_method_from_name_type<int (*)(Il2CppObject*)>(texture->klass, "get_height", 0)->methodPointer(texture);

			auto renderTexture = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(int, int, int, int, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "GetTemporary", 5)(width, height, 0, 0, 1);

			il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "Graphics", "Blit", 2)(texture, renderTexture);

			auto previous = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)()>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "get_active", -1)();

			il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "set_active", 1)(renderTexture);

			auto readableTexture = il2cpp_object_new(il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
			il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*, int, int)>(readableTexture->klass, ".ctor", 2)->methodPointer(readableTexture, width, height);

			il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*, Rect_t, int, int)>(readableTexture->klass, "ReadPixels", 3)->methodPointer(readableTexture, Rect_t{ 0, 0, static_cast<float>(width), static_cast<float>(height) }, 0, 0);
			il2cpp_class_get_method_from_name_type<void (*)(Il2CppObject*)>(readableTexture->klass, "Apply", 0)->methodPointer(readableTexture);

			il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "set_active", 1)(previous);

			il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*)>("UnityEngine.CoreModule.dll", "UnityEngine", "RenderTexture", "ReleaseTemporary", 1)(renderTexture);

			auto method = il2cpp_symbols::get_method("UnityEngine.ImageConversionModule.dll", "UnityEngine", "ImageConversion", "EncodeToPNG", 1);

			void** params = new void* [1];
			params[0] = readableTexture;

			Il2CppException* exception;

			auto pngData = reinterpret_cast<Il2CppArraySize_t<char>*>(il2cpp_runtime_invoke(method, nullptr, params, &exception));

			if (exception)
			{
				il2cpp_raise_exception(exception);
				return reinterpret_cast<decltype(ImageConversion_LoadImage_hook)*>(ImageConversion_LoadImage_orig)(tex, data, markNonReadable);
			}

			return reinterpret_cast<decltype(ImageConversion_LoadImage_hook)*>(ImageConversion_LoadImage_orig)(tex, pngData, markNonReadable);
		}

		return reinterpret_cast<decltype(ImageConversion_LoadImage_hook)*>(ImageConversion_LoadImage_orig)(tex, data, markNonReadable);
	}

	void* assetbundle_unload_orig = nullptr;
	void assetbundle_unload_hook(Il2CppObject* _this, bool unloadAllLoadedObjects)
	{
		reinterpret_cast<decltype(assetbundle_unload_hook)*>(assetbundle_unload_orig)(_this, unloadAllLoadedObjects);
		for (auto& pair : g_replace_assets)
		{
			if (pair.second.asset == _this)
			{
				pair.second.asset = nullptr;
			}
		}
	}

	void* AssetBundleRequest_GetResult_orig = nullptr;
	Il2CppObject* AssetBundleRequest_GetResult_hook(Il2CppObject* _this)
	{
		auto obj = reinterpret_cast<decltype(AssetBundleRequest_GetResult_hook)*>(AssetBundleRequest_GetResult_orig)(_this);
		if (obj)
		{
			auto name = uobject_get_name(obj);
			auto u8Name = local::wide_u8(name->start_char);
			if (find(replaceAssetNames.begin(), replaceAssetNames.end(), u8Name) != replaceAssetNames.end())
			{
				return GetReplacementAssets(name, il2cpp_class_get_type(obj->klass));
			}
		}
		return obj;
	}

	Il2CppObject* resources_load_hook(Il2CppString* path, Il2CppObject* type);

	Il2CppArraySize_t<Il2CppObject*>* GetRectTransformArray(Il2CppObject* object)
	{
		auto getComponents = il2cpp_class_get_method_from_name_type<Il2CppArraySize_t<Il2CppObject*> *(*)(Il2CppObject*, Il2CppType*, bool, bool, bool, bool, Il2CppObject*)>(object->klass, "GetComponentsInternal", 6)->methodPointer;
		auto rectTransformArray = getComponents(object, reinterpret_cast<Il2CppType*>(GetRuntimeType(
			"UnityEngine.CoreModule.dll", "UnityEngine", "RectTransform")), true, true, false, false, nullptr);

		return rectTransformArray;
	}

	Il2CppObject* GetRectTransform(Il2CppObject* object)
	{
		auto getComponents = il2cpp_class_get_method_from_name_type<Il2CppArraySize_t<Il2CppObject*> *(*)(Il2CppObject*, Il2CppType*, bool, bool, bool, bool, Il2CppObject*)>(object->klass, "GetComponentsInternal", 6)->methodPointer;
		auto rectTransformArray = getComponents(object, reinterpret_cast<Il2CppType*>(GetRuntimeType(
			"UnityEngine.CoreModule.dll", "UnityEngine", "RectTransform")), true, true, false, false, nullptr);

		if (rectTransformArray->max_length)
		{
			return rectTransformArray->vector[0];
		}

		return nullptr;
	}

	Il2CppObject* CreateGameObject()
	{
		auto gameObjectClass = il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
		auto gameObject = il2cpp_object_new(gameObjectClass);
		il2cpp_runtime_object_init(gameObject);

		return gameObject;
	}

	Il2CppObject* AddComponent(Il2CppObject* gameObject, void* componentType)
	{
		return il2cpp_resolve_icall_type<Il2CppObject * (*)(Il2CppObject*, void*)>("UnityEngine.GameObject::Internal_AddComponentWithType()")(gameObject, componentType);
	}

	void* Object_Internal_CloneSingleWithParent_orig = nullptr;
	Il2CppObject* Object_Internal_CloneSingleWithParent_hook(Il2CppObject* data, Il2CppObject* parent, bool worldPositionStays)
	{
		auto cloned = reinterpret_cast<decltype(Object_Internal_CloneSingleWithParent_hook)*>(Object_Internal_CloneSingleWithParent_orig)(data, parent, worldPositionStays);

		return cloned;
	}

	void* resources_load_orig = nullptr;
	Il2CppObject* resources_load_hook(Il2CppString* path, Il2CppObject* type)
	{
		string u8Name = local::wide_u8(path->start_char);
		if (u8Name == "TMP Settings"s && g_replace_to_custom_font && fontAssets)
		{
			auto object = reinterpret_cast<decltype(resources_load_hook)*>(resources_load_orig)(path, type);
			auto fontAssetField = il2cpp_class_get_field_from_name(object->klass, "m_defaultFontAsset");
			il2cpp_field_set_value(object, fontAssetField, GetCustomTMPFont());
			return object;
		}

		return reinterpret_cast<decltype(resources_load_hook)*>(resources_load_orig)(path, type);

	}

	void* Sprite_get_texture_orig = nullptr;
	Il2CppObject* Sprite_get_texture_hook(Il2CppObject* _this)
	{
		auto texture2D = reinterpret_cast<decltype(Sprite_get_texture_hook)*>(Sprite_get_texture_orig)(_this);
		auto uobject_name = uobject_get_name(texture2D);
		if (!local::wide_u8(uobject_name->start_char).empty())
		{
			auto newTexture = GetReplacementAssets(
				uobject_name,
				(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
			if (newTexture)
			{
				il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
					(newTexture, 32);
				return newTexture;
			}
		}
		return texture2D;
	}

	// void* Renderer_get_material_orig = nullptr;
	Il2CppObject* Renderer_get_material_hook(Il2CppObject* _this)
	{
		auto material = reinterpret_cast<decltype(Renderer_get_material_hook)*>(Renderer_get_material_orig)(_this);
		if (material)
		{
			ReplaceMaterialTexture(material);
		}
		return material;
	}

	void* Renderer_get_materials_orig = nullptr;
	Il2CppArraySize* Renderer_get_materials_hook(Il2CppObject* _this)
	{
		auto materials = reinterpret_cast<decltype(Renderer_get_materials_hook)*>(Renderer_get_materials_orig)(_this);
		for (int i = 0; i < materials->max_length; i++)
		{
			auto material = (Il2CppObject*)materials->vector[i];
			if (material)
			{
				ReplaceMaterialTexture(material);
			}
		}
		return materials;
	}

	void* Renderer_get_sharedMaterial_orig = nullptr;
	Il2CppObject* Renderer_get_sharedMaterial_hook(Il2CppObject* _this)
	{
		auto material = reinterpret_cast<decltype(Renderer_get_sharedMaterial_hook)*>(Renderer_get_sharedMaterial_orig)(_this);
		if (material)
		{
			ReplaceMaterialTexture(material);
		}
		return material;
	}

	void* Renderer_get_sharedMaterials_orig = nullptr;
	Il2CppArraySize* Renderer_get_sharedMaterials_hook(Il2CppObject* _this)
	{
		auto materials = reinterpret_cast<decltype(Renderer_get_sharedMaterials_hook)*>(Renderer_get_sharedMaterials_orig)(_this);
		for (int i = 0; i < materials->max_length; i++)
		{
			auto material = (Il2CppObject*)materials->vector[i];
			if (material)
			{
				ReplaceMaterialTexture(material);
			}
		}
		return materials;
	}

	void* Renderer_set_material_orig = nullptr;
	void Renderer_set_material_hook(Il2CppObject* _this, Il2CppObject* material)
	{
		if (material)
		{
			ReplaceMaterialTexture(material);
		}
		reinterpret_cast<decltype(Renderer_set_material_hook)*>(Renderer_set_material_orig)(_this, material);
	}

	void* Renderer_set_materials_orig = nullptr;
	void Renderer_set_materials_hook(Il2CppObject* _this, Il2CppArraySize* materials)
	{
		for (int i = 0; i < materials->max_length; i++)
		{
			auto material = (Il2CppObject*)materials->vector[i];
			if (material)
			{
				ReplaceMaterialTexture(material);
			}
		}
		reinterpret_cast<decltype(Renderer_set_materials_hook)*>(Renderer_set_materials_orig)(_this, materials);
	}

	void* Renderer_set_sharedMaterial_orig = nullptr;
	void Renderer_set_sharedMaterial_hook(Il2CppObject* _this, Il2CppObject* material)
	{
		if (material)
		{
			ReplaceMaterialTexture(material);
		}
		reinterpret_cast<decltype(Renderer_set_sharedMaterial_hook)*>(Renderer_set_sharedMaterial_orig)(_this, material);
	}

	void* Renderer_set_sharedMaterials_orig = nullptr;
	void Renderer_set_sharedMaterials_hook(Il2CppObject* _this, Il2CppArraySize* materials)
	{
		for (int i = 0; i < materials->max_length; i++)
		{
			auto material = (Il2CppObject*)materials->vector[i];
			if (material)
			{
				ReplaceMaterialTexture(material);
			}
		}
		reinterpret_cast<decltype(Renderer_set_sharedMaterials_hook)*>(Renderer_set_sharedMaterials_orig)(_this, materials);
	}

	void* Material_set_mainTexture_orig = nullptr;
	void Material_set_mainTexture_hook(Il2CppObject* _this, Il2CppObject* texture)
	{
		if (texture)
		{
			if (!local::wide_u8(uobject_get_name(texture)->start_char).empty())
			{
				auto newTexture = GetReplacementAssets(
					uobject_get_name(texture),
					(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
				if (newTexture)
				{
					il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
						(newTexture, 32);
					reinterpret_cast<decltype(Material_set_mainTexture_hook)*>(Material_set_mainTexture_orig)(_this, newTexture);
					return;
				}
			}
		}
		reinterpret_cast<decltype(Material_set_mainTexture_hook)*>(Material_set_mainTexture_orig)(_this, texture);
	}

	void* Material_get_mainTexture_orig = nullptr;
	Il2CppObject* Material_get_mainTexture_hook(Il2CppObject* _this)
	{
		auto texture = reinterpret_cast<decltype(Material_get_mainTexture_hook)*>(Material_get_mainTexture_orig)(_this);
		if (texture)
		{
			auto uobject_name = uobject_get_name(texture);
			if (!local::wide_u8(uobject_name->start_char).empty())
			{
				auto newTexture = GetReplacementAssets(
					uobject_name,
					(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
				if (newTexture)
				{
					il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
						(newTexture, 32);
					return newTexture;
				}
			}
		}
		return texture;
	}

	void* Material_SetTextureI4_orig = nullptr;
	void Material_SetTextureI4_hook(Il2CppObject* _this, int nameID, Il2CppObject* texture)
	{
		if (texture && !local::wide_u8(uobject_get_name(texture)->start_char).empty())
		{
			auto newTexture = GetReplacementAssets(
				uobject_get_name(texture),
				(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
			if (newTexture)
			{
				il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
					(newTexture, 32);
				reinterpret_cast<decltype(Material_SetTextureI4_hook)*>(Material_SetTextureI4_orig)(_this, nameID, newTexture);
				return;
			}
		}
		reinterpret_cast<decltype(Material_SetTextureI4_hook)*>(Material_SetTextureI4_orig)(_this, nameID, texture);
	}

	// void* Material_GetTextureImpl_orig = nullptr;
	Il2CppObject* Material_GetTextureImpl_hook(Il2CppObject* _this, int nameID)
	{
		auto texture = reinterpret_cast<decltype(Material_GetTextureImpl_hook)*>(Material_GetTextureImpl_orig)(_this, nameID);
		if (texture && !local::wide_u8(uobject_get_name(texture)->start_char).empty())
		{
			auto newTexture = GetReplacementAssets(
				uobject_get_name(texture),
				(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
			if (newTexture)
			{
				il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
					(newTexture, 32);
				return newTexture;
			}
		}
		return texture;
	}

	void* Material_SetTextureImpl_orig = nullptr;
	void Material_SetTextureImpl_hook(Il2CppObject* _this, int nameID, Il2CppObject* texture)
	{
		if (texture && !local::wide_u8(uobject_get_name(texture)->start_char).empty())
		{
			auto newTexture = GetReplacementAssets(
				uobject_get_name(texture),
				(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
			if (newTexture)
			{
				il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
					(newTexture, 32);
				reinterpret_cast<decltype(Material_SetTextureImpl_hook)*>(Material_SetTextureImpl_orig)(_this, nameID, newTexture);
				return;
			}
		}
		reinterpret_cast<decltype(Material_SetTextureImpl_hook)*>(Material_SetTextureImpl_orig)(_this, nameID, texture);
	}

	void* RawImage_get_mainTexture_orig = nullptr;
	Il2CppObject* RawImage_get_mainTexture_hook(Il2CppObject* _this)
	{
		auto texture = reinterpret_cast<decltype(RawImage_get_mainTexture_hook)*>(RawImage_get_mainTexture_orig)(_this);
		if (texture)
		{
			auto uobject_name = uobject_get_name(texture);
			if (!local::wide_u8(uobject_name->start_char).empty())
			{
				auto newTexture = GetReplacementAssets(
					uobject_name,
					(Il2CppType*)GetRuntimeType("UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D"));
				if (newTexture)
				{
					il2cpp_symbols::get_method_pointer<void (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Object", "set_hideFlags", 1)
						(newTexture, 32);
					return newTexture;
				}
			}
		}
		return texture;
	}

	void* GameObject_GetComponent_orig = nullptr;
	Il2CppObject* GameObject_GetComponent_hook(Il2CppObject* _this, const Il2CppType* type)
	{
		auto component = reinterpret_cast<decltype(GameObject_GetComponent_hook)*>(GameObject_GetComponent_orig)(_this, type);
		if (component)
		{
			// cout << "Component: " << component->klass->name << endl;
			if ("AssetHolder"s == component->klass->name)
			{
				ReplaceAssetHolderTextures(component);
			}
		}
		return component;
	}

	struct CastHelper
	{
		Il2CppObject* obj;
		uintptr_t oneFurtherThanResultValue;
	};

	void* GameObject_GetComponentFastPath_orig = nullptr;
	void GameObject_GetComponentFastPath_hook(Il2CppObject* _this, Il2CppObject* type, uintptr_t oneFurtherThanResultValue)
	{
		/*auto name = il2cpp_class_get_method_from_name_type<Il2CppString* (*)(Il2CppObject*)>(type->klass, "get_FullName", 0)->methodPointer(type);
		cout << "GameObject_GetComponentFastPath " << local::wide_u8(name->start_char) << endl;*/
		reinterpret_cast<decltype(GameObject_GetComponentFastPath_hook)*>(GameObject_GetComponentFastPath_orig)(_this, type, oneFurtherThanResultValue);
		auto helper = CastHelper{};
		int objSize = sizeof(helper.obj);
		memmove(&helper, reinterpret_cast<void*>(oneFurtherThanResultValue - objSize), sizeof(CastHelper));
		if (helper.obj)
		{
			// cout << "Helper " << helper.obj->klass->name << endl;
			if (string(helper.obj->klass->name).find("MeshRenderer") != string::npos)
			{
				ReplaceRendererTexture(helper.obj);
			}
			if (string(helper.obj->klass->name).find("RawImage") != string::npos)
			{
				ReplaceRawImageTexture(helper.obj);
			}
			if (helper.obj->klass->name == "Image"s)
			{
				auto material = il2cpp_class_get_method_from_name_type<Il2CppObject * (*)(Il2CppObject*)>(helper.obj->klass, "get_material", 0)->methodPointer(helper.obj);
				if (material)
				{
					ReplaceMaterialTexture(material);
				}
			}
			if (helper.obj->klass->name == "AssetHolder"s)
			{
				ReplaceAssetHolderTextures(helper.obj);
			}
			memmove(reinterpret_cast<void*>(oneFurtherThanResultValue - objSize), &helper, sizeof(CastHelper));
		}
	}

	void* CriMana_Player_SetFile_orig = nullptr;
	bool CriMana_Player_SetFile_hook(Il2CppObject* _this, Il2CppObject* binder, Il2CppString* moviePath, int setMode)
	{
		stringstream pathStream(local::wide_u8(moviePath->start_char));
		string segment;
		vector<string> splited;
		while (getline(pathStream, segment, '\\'))
		{
			splited.emplace_back(segment);
		}
		if (g_replace_assets.find(splited[splited.size() - 1]) != g_replace_assets.end())
		{
			auto& replaceAsset = g_replace_assets.at(splited[splited.size() - 1]);
			moviePath = il2cpp_string_new(replaceAsset.path.data());
		}
		return reinterpret_cast<decltype(CriMana_Player_SetFile_hook)*>(CriMana_Player_SetFile_orig)(_this, binder, moviePath, setMode);
	}

	string GetLoginURL()
	{
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		LPSTR pszOutBuffer;
		BOOL  bResults = FALSE;
		HINTERNET  hSession = NULL,
			hConnect = NULL,
			hRequest = NULL;

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen(L"WinHTTP/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);

		// Specify an HTTP server.
		if (hSession)
			hConnect = WinHttpConnect(hSession, L"apidgp-gameplayer.games.dmm.com",
				INTERNET_DEFAULT_HTTPS_PORT, 0);

		auto acceptType = L"application/json";

		// Create an HTTP request handle.
		if (hConnect)
			hRequest = WinHttpOpenRequest(hConnect, L"GET", L"/v5/loginurl",
				NULL, WINHTTP_NO_REFERER,
				&acceptType,
				WINHTTP_FLAG_SECURE);

		// Send a request.
		if (hRequest)
			bResults = WinHttpSendRequest(hRequest,
				WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0,
				0, 0);

		// End the request.
		if (bResults)
			bResults = WinHttpReceiveResponse(hRequest, NULL);

		string jsonData;

		// Keep checking for data until there is nothing left.
		if (bResults)
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				{
					cout << "Error " << GetLastError() << " in WinHttpQueryDataAvailable." << endl;
				}

				// Allocate space for the buffer.
				pszOutBuffer = new char[dwSize + 1];
				if (!pszOutBuffer)
				{
					cout << "Out of memory" << endl;
					dwSize = 0;
				}
				else
				{
					// Read the data.
					ZeroMemory(pszOutBuffer, dwSize + 1);

					if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
						dwSize, &dwDownloaded))
					{
						cout << "Error " << GetLastError() << " in WinHttpReadData." << endl;
					}
					else
					{
						if (strlen(pszOutBuffer) > 0)
						{
							jsonData = pszOutBuffer;
						}
					}

					// Free the memory allocated to the buffer.
					delete[] pszOutBuffer;
				}
			} while (dwSize > 0);
		}


		// Report any errors.
		if (!bResults)
		{
			cout << "Error " << GetLastError() << " has occurred." << endl;
		}

		// Close any open handles.
		if (hRequest) WinHttpCloseHandle(hRequest);
		if (hConnect) WinHttpCloseHandle(hConnect);
		if (hSession) WinHttpCloseHandle(hSession);

		rapidjson::Document document;

		document.Parse(jsonData.data());

		if (document.HasParseError())
		{
			cout << "Response JSON parse error: " << GetParseError_En(document.GetParseError()) << " (" << document.GetErrorOffset() << ")" << endl;
			return "";
		}

		if (document.HasMember("result_code") && document["result_code"].GetInt() == 100)
		{
			return string(document["data"].GetObjectA()["url"].GetString());
		}

		return "";
	}

	string GetGameArgs(wstring sessionId, wstring secureId)
	{
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		LPSTR pszOutBuffer;
		BOOL  bResults = FALSE;
		HINTERNET  hSession = NULL,
			hConnect = NULL,
			hRequest = NULL;

		// Use WinHttpOpen to obtain a session handle.
		hSession = WinHttpOpen(L"WinHTTP/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);

		// Specify an HTTP server.
		if (hSession)
		{
			hConnect = WinHttpConnect(hSession, L"apidgp-gameplayer.games.dmm.com",
				INTERNET_DEFAULT_HTTPS_PORT, 0);
		}
		else
		{
			cout << "Connect create failed" << endl;
		}

		auto acceptType = L"application/json";

		// Create an HTTP request handle.
		if (hConnect)
			hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/v5/launch/cl",
				NULL, WINHTTP_NO_REFERER,
				WINHTTP_DEFAULT_ACCEPT_TYPES,
				WINHTTP_FLAG_SECURE);

		// Send a request.
		if (hRequest)
		{
			WinHttpAddRequestHeaders(hRequest, L"Client-App: DMMGamePlayer5", static_cast<unsigned long>(-1), WINHTTP_ADDREQ_FLAG_ADD);
			WinHttpAddRequestHeaders(hRequest, L"Client-version: 5.2.31", static_cast<unsigned long>(-1), WINHTTP_ADDREQ_FLAG_ADD);
			WinHttpAddRequestHeaders(hRequest, L"Content-Type: application/json", static_cast<unsigned long>(-1), WINHTTP_ADDREQ_FLAG_ADD);

			wstringstream sessionCookieStream;
			sessionCookieStream << L"Cookie: login_session_id=" << sessionId << L";login_secure_id=" << secureId;

			auto body = R"({"product_id":"imasscprism","game_type":"GCL","launch_type":"LIB","game_os":"win","user_os":"win","mac_address":"null","hdd_serial":"null","motherboard":"null"})"s;

			WinHttpAddRequestHeaders(hRequest, sessionCookieStream.str().data(), -1, WINHTTP_ADDREQ_FLAG_ADD);

			bResults = WinHttpSendRequest(hRequest,
				NULL, NULL,
				reinterpret_cast<LPVOID>(const_cast<LPSTR>(body.data())), body.size(),
				body.size(), NULL);
		}
		else
		{
			cout << "Request create failed" << endl;
		}

		// End the request.
		if (bResults)
			bResults = WinHttpReceiveResponse(hRequest, NULL);

		string jsonData;

		// Keep checking for data until there is nothing left.
		if (bResults)
		{
			do
			{
				// Check for available data.
				dwSize = 0;
				if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				{
					cout << "Error " << GetLastError() << " in WinHttpQueryDataAvailable." << endl;
				}

				// Allocate space for the buffer.
				pszOutBuffer = new char[dwSize + 1];
				if (!pszOutBuffer)
				{
					cout << "Out of memory" << endl;
					dwSize = 0;
				}
				else
				{
					// Read the data.
					ZeroMemory(pszOutBuffer, dwSize + 1);

					if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
						dwSize, &dwDownloaded))
					{
						cout << "Error " << GetLastError() << " in WinHttpReadData." << endl;
					}
					else
					{
						if (strlen(pszOutBuffer) > 0)
						{
							jsonData = pszOutBuffer;
						}
					}

					// Free the memory allocated to the buffer.
					delete[] pszOutBuffer;
				}
			} while (dwSize > 0);
		}


		// Report any errors.
		if (!bResults)
		{
			cout << "Error " << GetLastError() << " has occurred." << endl;
		}

		// Close any open handles.
		if (hRequest) WinHttpCloseHandle(hRequest);
		if (hConnect) WinHttpCloseHandle(hConnect);
		if (hSession) WinHttpCloseHandle(hSession);

		rapidjson::Document document;

		document.Parse(jsonData.data());

		if (document.HasParseError())
		{
			cout << "Response JSON parse error: " << GetParseError_En(document.GetParseError()) << " (" << document.GetErrorOffset() << ")" << endl;
			return "";
		}

		if (document.HasMember("result_code") && document["result_code"].GetInt() == 100)
		{
			return string(document["data"].GetObjectA()["execute_args"].GetString());
		}

		return "";
	}

	wil::com_ptr<ICoreWebView2Controller> webviewController;
	wil::com_ptr<ICoreWebView2> webview;

	bool isLoginWebViewOpen = false;

	string viewerId;
	string onetimeToken;

	LRESULT CALLBACK WebViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_SIZE:
			if (webviewController != nullptr) {
				RECT bounds;
				GetClientRect(hWnd, &bounds);
				webviewController->put_Bounds(bounds);
			};
			break;
		case WM_DESTROY:
			isLoginWebViewOpen = false;
			PostQuitMessage(0);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
			break;
		}

		return NULL;
	}

	DWORD WINAPI WebViewThread(LPVOID)
	{
		IsGUIThread(TRUE);

		WNDCLASSEX wcex = {};

		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WebViewWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, (LPCSTR)0x67);
		wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wcex.lpszMenuName = NULL;
		wcex.lpszClassName = "WebViewWindow";
		wcex.hIconSm = LoadIcon(wcex.hInstance, reinterpret_cast<LPCSTR>(0x67));

		RegisterClassEx(&wcex);

		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);

		const auto hWnd = CreateWindowEx(NULL, "WebViewWindow", "Login DMM",
			WS_OVERLAPPEDWINDOW ^ WS_MAXIMIZEBOX ^ WS_MINIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT,
			900, 900,
			nullptr, NULL, hInstance, NULL);

		ShowWindow(hWnd, SW_SHOWDEFAULT);
		UpdateWindow(hWnd);

		auto envOptions = Make<CoreWebView2EnvironmentOptions>();
#ifdef _DEBUG
		envOptions->put_AdditionalBrowserArguments(L"--enable-logging --v=1");
#endif

		auto loginUrl = GetLoginURL();

		CreateCoreWebView2EnvironmentWithOptions(nullptr, L"./WebView2", envOptions.Get(),
			Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
				[hWnd, loginUrl](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
				{
					env->CreateCoreWebView2Controller(hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
						[env, hWnd, loginUrl](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT
						{
							if (controller != nullptr)
							{
								webviewController = controller;
								webviewController->get_CoreWebView2(&webview);
							}
							else
							{
								return S_OK;
							}

							ICoreWebView2Settings* settings;
							webview->get_Settings(&settings);
							settings->put_IsScriptEnabled(TRUE);
							settings->put_AreDefaultScriptDialogsEnabled(TRUE);
							settings->put_IsWebMessageEnabled(TRUE);

							RECT bounds;
							GetClientRect(hWnd, &bounds);

							webviewController->put_Bounds(bounds);

							webview->Navigate(local::u8_wide(loginUrl).data());

#ifdef _DEBUG
							webview->OpenDevToolsWindow();
#endif

							EventRegistrationToken token;
							webview->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
								[hWnd](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
								{
									LPWSTR uri;
									args->get_Uri(&uri);
									std::wstring source(uri);

									if (source == L"dmmgameplayer://showLibrary")
									{
										ICoreWebView2_2* webView2;
										webview->QueryInterface<ICoreWebView2_2>(&webView2);

										ICoreWebView2CookieManager* cookieManager;
										webView2->get_CookieManager(&cookieManager);

										cookieManager->GetCookies(L"https://accounts.dmm.com", Callback<ICoreWebView2GetCookiesCompletedHandler>(
											[hWnd](
												HRESULT result,
												ICoreWebView2CookieList* cookieList)
											{
												UINT count;
												cookieList->get_Count(&count);


												wstring sessionId;
												wstring secureId;


												for (int i = 0; i < count; i++)
												{
													ICoreWebView2Cookie* cookie;
													cookieList->GetValueAtIndex(i, &cookie);

													LPWSTR name;
													cookie->get_Name(&name);

													LPWSTR value;

													if (name == L"login_session_id"s)
													{
														cookie->get_Value(&value);
														sessionId = value;
													}

													if (name == L"login_secure_id"s)
													{
														cookie->get_Value(&value);
														secureId = value;
													}

													if (!sessionId.empty() && !secureId.empty())
													{
														break;
													}
												}

												auto gameArgs = GetGameArgs(sessionId, secureId);

												stringstream gameArgsStream(gameArgs);
												string segment;
												vector<string> split;
												while (getline(gameArgsStream, segment, ' '))
												{
													split.emplace_back(segment);
												}

												stringstream singleArgStream1(split[0]);
												vector<string> split1;
												while (getline(singleArgStream1, segment, '='))
												{
													split1.emplace_back(segment);
												}

												viewerId = string(split1.back());

												split1.clear();

												stringstream singleArgStream2(split[1]);
												while (getline(singleArgStream2, segment, '='))
												{
													split1.emplace_back(segment);
												}

												onetimeToken = string(split1.back());


												webviewController->Close();
												PostMessage(hWnd, WM_CLOSE, NULL, NULL);

												isLoginWebViewOpen = false;

												return S_OK;
											}).Get());
									}

									if (source.substr(0, 5) != L"https")
									{
										args->put_Cancel(true);
									}
									return S_OK;
								}).Get(), &token);


							return S_OK;
						}
					).Get());
					return S_OK;
				}
		).Get());


		MSG msg;
		while (GetMessage(&msg, nullptr, 0, 0) && isLoginWebViewOpen)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		return msg.wParam;
	}

	void DumpLocalization(string fileName = "localization.json")
	{
		if (g_dump_localization)
		{
			auto localizationManager = GetSingletonInstance(il2cpp_symbols::get_class("ENTERPRISE.Localization.dll", "ENTERPRISE.Localization", "LocalizationManager"));

			auto dicField = il2cpp_class_get_field_from_name(localizationManager->klass, "dic");
			Il2CppObject* dic;
			il2cpp_field_get_value(localizationManager, dicField, &dic);

			if (dic)
			{
				auto entriesField = il2cpp_class_get_field_from_name(dic->klass, "_entries");
				Il2CppArraySize_t<Entry<Il2CppString*, Il2CppObject*>>* entries;
				il2cpp_field_get_value(dic, entriesField, &entries);

				rapidjson::Document document(rapidjson::kObjectType);

				for (int i = 0; i < entries->max_length; i++)
				{
					auto entry = entries->vector[i];

					if (entry.key)
					{
						rapidjson::Value object;
						object.SetObject();

						auto entries1Field = il2cpp_class_get_field_from_name(entry.value->klass, "_entries");
						Il2CppArraySize_t<Entry<int, Il2CppString*>>* entries1;
						il2cpp_field_get_value(entry.value, entries1Field, &entries1);

						for (int i = 0; i < entries1->max_length; i++)
						{
							auto entry1 = entries1->vector[i];

							rapidjson::Value value(to_string(entry1.key).data(), document.GetAllocator());

							if (entry1.key)
							{
								if (entry1.value)
								{
									auto u8String = local::wide_u8(entry1.value->start_char);

									object.AddMember(value, u8String, document.GetAllocator());
								}
								else
								{
									object.AddMember(value, rapidjson::Value(rapidjson::kNullType), document.GetAllocator());
								}
							}
						}
						rapidjson::Value value(local::wide_u8(entry.key->start_char).data(), document.GetAllocator());

						document.AddMember(value, object, document.GetAllocator());
					}
				}

				rapidjson::StringBuffer buffer;
				buffer.Clear();
				rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
				document.Accept(writer);

				fstream localization;
				localization.open(fileName, ios::out);
				localization << buffer.GetString() << endl;
				localization.close();
			}
		}
	}

	void OverwriteLocalization()
	{
		if (!localization_dict.IsNull() && !localization_dict.ObjectEmpty())
		{
			auto localizationManager = GetSingletonInstance(il2cpp_symbols::get_class("ENTERPRISE.Localization.dll", "ENTERPRISE.Localization", "LocalizationManager"));

			auto dicField = il2cpp_class_get_field_from_name(localizationManager->klass, "dic");
			Il2CppObject* dic;
			il2cpp_field_get_value(localizationManager, dicField, &dic);

			if (dic)
			{
				auto entriesField = il2cpp_class_get_field_from_name(dic->klass, "_entries");
				Il2CppArraySize_t<Entry<Il2CppString*, Il2CppObject*>>* entries;
				il2cpp_field_get_value(dic, entriesField, &entries);

				for (int i = 0; i < entries->max_length; i++)
				{
					auto entry = entries->vector[i];

					if (entry.key)
					{
						auto keyU8 = local::wide_u8(entry.key->start_char);

						if (localization_dict.HasMember(keyU8))
						{
							auto dict = localization_dict[keyU8].GetObjectA();

							auto entries1Field = il2cpp_class_get_field_from_name(entry.value->klass, "_entries");
							Il2CppArraySize_t<Entry<int, Il2CppString*>>* entries1;
							il2cpp_field_get_value(entry.value, entries1Field, &entries1);

							auto newEntries = il2cpp_array_new(il2cpp_class_from_type(entries1Field->type), entries1->max_length);

							for (int i = 0; i < entries1->max_length; i++)
							{
								auto entry1 = entries1->vector[i];

								auto innerKeyU8 = to_string(entry1.key);

								if (dict.HasMember(innerKeyU8))
								{
									void** params = new void* [2];
									params[0] = &entry1.key;
									params[1] = il2cpp_string_new(dict[innerKeyU8].GetString());

									Il2CppException* exception;
									il2cpp_runtime_invoke(il2cpp_class_get_method_from_name(entry.value->klass, "set_Item", 2), entry.value, params, &exception);

									if (exception)
									{
										il2cpp_raise_exception(exception);
									}
								}
							}
						}
					}
				}
			}
		}
	}

	void* LocalizationManager_LoadAsync_orig = nullptr;
	UniTask LocalizationManager_LoadAsync_hook(Il2CppObject* _this, Il2CppObject* cancellationToken)
	{
		auto task = reinterpret_cast<decltype(LocalizationManager_LoadAsync_hook)*>(LocalizationManager_LoadAsync_orig)(_this, cancellationToken);

		DumpLocalization();

		OverwriteLocalization();

		return task;
	}

	void* TimeUtility_ToExpiredTimeText_orig = nullptr;
	Il2CppString* TimeUtility_ToExpiredTimeText_hook(Il2CppObject* iExpiredTime, bool isShowSeconds)
	{
		auto text = reinterpret_cast<decltype(TimeUtility_ToExpiredTimeText_hook)*>(TimeUtility_ToExpiredTimeText_orig)(iExpiredTime, isShowSeconds);
		return text;
	}

	void* Global_SystemReset_orig = nullptr;
	void Global_SystemReset_hook()
	{
		reinterpret_cast<decltype(Global_SystemReset_hook)*>(Global_SystemReset_orig)();

		OverwriteLocalization();
	}

	void* CatalogDB_ctor_orig = nullptr;
	void CatalogDB_ctor_hook(Il2CppObject* _this, Il2CppString* filePath, Il2CppArraySize_t<uint8_t>* seed)
	{
		wcout << L"SQLite Encrypted: " << filePath->start_char << endl;

		if (seed)
		{
			cout << "seed size: " << seed->max_length << endl;
			for (int i = 0; i < seed->max_length; i++)
			{
				printf("%02X ", seed->vector[i]);
				seed->vector[i] = 0;
			}
			cout << dec << endl;
		}

		reinterpret_cast<decltype(CatalogDB_ctor_hook)*>(CatalogDB_ctor_orig)(_this, filePath, seed);
	}

	void* TweenManager_Update_orig = nullptr;
	void* TweenManager_Update_hook(Il2CppObject* _this, void* updateType, float deltaTime, float independentTime)
	{
		return reinterpret_cast<decltype(TweenManager_Update_hook)*>(TweenManager_Update_orig)(_this, updateType, deltaTime * g_ui_animation_scale, independentTime * g_ui_animation_scale);
	}

	void* LiveMVPresenter_OnApplicationFocus_orig = nullptr;
	void LiveMVPresenter_OnApplicationFocus_hook(Il2CppObject* _this, bool hasFocus)
	{
		reinterpret_cast<decltype(LiveMVPresenter_OnApplicationFocus_hook)*>(LiveMVPresenter_OnApplicationFocus_orig)(_this, true);
	}

	void* load_scene_internal_orig = nullptr;
	void* load_scene_internal_hook(Il2CppString* sceneName, int sceneBuildIndex, void* parameters, bool mustCompleteNextFrame)
	{
		wprintf(L"%s\n", sceneName->start_char);
		return reinterpret_cast<decltype(load_scene_internal_hook)*>(load_scene_internal_orig)(sceneName, sceneBuildIndex, parameters, mustCompleteNextFrame);
	}

	void dump_all_entries()
	{
		vector<wstring> static_entries;
		// 0 is None
		for (int i = 1;; i++)
		{
			//auto* str = reinterpret_cast<decltype(localize_get_hook)*>(localize_get_orig)(i);

			//if (str && *str->start_char)
			//{
			//	if (g_static_entries_use_hash)
			//	{
			//		static_entries.emplace_back(str->start_char);
			//	}
			//	else
			//	{
			//		logger::write_entry(i, str->start_char);
			//	}
			//}
			//else
			//{
			//	// check next string, if it's still empty, then we are done!
			//	auto* nextStr = reinterpret_cast<decltype(localize_get_hook)*>(localize_get_orig)(i + 1);
			//	if (!(nextStr && *nextStr->start_char))
			//		break;
			//}
			break;
		}
	}

	void path_game_assembly()
	{
		if (!mh_inited)
			return;

		printf("Trying to patch GameAssembly.dll...\n");

		auto il2cpp_module = GetModuleHandle("GameAssembly.dll");

		// load il2cpp exported functions
		il2cpp_symbols::init(il2cpp_module);


		if (g_dump_il2cpp)
		{
			il2cpp_dump();
		}

#pragma region HOOK_MACRO
#define ADD_HOOK(_name_, _fmt_) \
	auto _name_##_offset = reinterpret_cast<void*>(_name_##_addr); \
	\
	printf(_fmt_, _name_##_offset); \
	dump_bytes(_name_##_offset); \
	\
	auto _name_##_create_result = MH_CreateHook(_name_##_offset, _name_##_hook, &_name_##_orig); \
	if (_name_##_create_result != MH_OK) \
	{\
		cout << "WARN: " << #_name_ << " Create Failed: " << MH_StatusToString(_name_##_create_result) << endl; \
	}\
	auto _name_##_result = MH_EnableHook(_name_##_offset); \
	if (_name_##_result != MH_OK) \
	{\
		cout << "WARN: " << #_name_ << " Failed: " << MH_StatusToString(_name_##_result) << " LastError: " << GetLastError() << endl << endl; \
		_name_##_orig = _name_##_addr; \
	}
#pragma endregion
#pragma region HOOK_ADDRESSES
		load_assets = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppObject * _this, Il2CppString * name, Il2CppObject * runtimeType)>(
			"UnityEngine.AssetBundleModule.dll", "UnityEngine",
			"AssetBundle", "LoadAsset", 2);

		get_all_asset_names = il2cpp_resolve_icall_type<Il2CppArraySize * (*)(Il2CppObject * _this)>("UnityEngine.AssetBundle::GetAllAssetNames()");

		uobject_get_name = il2cpp_symbols::get_method_pointer<Il2CppString * (*)(Il2CppObject * uObject)>(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Object", "GetName", -1);

		uobject_set_name = il2cpp_symbols::get_method_pointer<Il2CppString * (*)(Il2CppObject * uObject, Il2CppString * name)>(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Object", "SetName", 2);

		uobject_IsNativeObjectAlive = il2cpp_symbols::get_method_pointer<bool (*)(Il2CppObject * uObject)>(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Object", "IsNativeObjectAlive", 1);

		get_unityVersion = il2cpp_symbols::get_method_pointer<Il2CppString * (*)()>(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Application", "get_unityVersion", -1);

		auto populate_with_errors_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.TextRenderingModule.dll",
			"UnityEngine", "TextGenerator",
			"PopulateWithErrors", 3
		);

		auto get_preferred_width_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.TextRenderingModule.dll",
			"UnityEngine", "TextGenerator",
			"GetPreferredWidth", 2
		);

		auto set_fps_addr = il2cpp_resolve_icall("UnityEngine.Application::set_targetFrameRate(System.Int32)");

		display_get_main = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)()>(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_main", -1);

		get_system_width = il2cpp_symbols::get_method_pointer<int (*)(Il2CppObject*)>(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_systemWidth", 0);

		get_system_height = il2cpp_symbols::get_method_pointer<int (*)(Il2CppObject*)>(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_systemHeight", 0);

		get_rendering_width = il2cpp_symbols::get_method_pointer<int (*)(Il2CppObject*)>(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_renderingWidth", 0);

		get_rendering_height = il2cpp_symbols::get_method_pointer<int (*)(Il2CppObject*)>(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_renderingHeight", 0);

		get_resolution = reinterpret_cast<decltype(get_resolution)>(il2cpp_resolve_icall("UnityEngine.Screen::get_currentResolution_Injected(UnityEngine.Resolution&)"));

		auto canvas_scaler_setres_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"CanvasScaler", "set_referenceResolution", 1
		);

		set_scale_factor = il2cpp_symbols::get_method_pointer<void(*)(void*, float)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"CanvasScaler", "set_scaleFactor", 1
		);

		auto on_populate_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "OnPopulateMesh", 1
		);

		auto TMP_Text_GetTextElement_addr = il2cpp_symbols::get_method_pointer(
			"Unity.TextMeshPro.dll", "TMPro",
			"TMP_Text", "GetTextElement", 5
		);

		auto UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.UI.dll", "ENTERPRISE.UI",
			"UITextMeshProUGUI", "get_text", 0
		);

		auto UITextMeshProUGUI_Awake_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.UI.dll", "ENTERPRISE.UI",
			"UITextMeshProUGUI", "Awake", 0
		);

		auto UITextMeshProUGUI_set_text_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.UI.dll", "ENTERPRISE.UI",
			"UITextMeshProUGUI", "set_text", 1
		);

		text_get_text = il2cpp_symbols::get_method_pointer<Il2CppString * (*)(void*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "get_text", 0
		);
		text_set_text = il2cpp_symbols::get_method_pointer<void (*)(void*, Il2CppString*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_text", 1
		);

		text_assign_font = il2cpp_symbols::get_method_pointer<void(*)(void*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "AssignDefaultFont", 0
		);

		text_set_font = il2cpp_symbols::get_method_pointer<void (*)(void*, Il2CppObject*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_font", 1
		);

		text_get_font = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(void*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "get_font", 0
		);

		text_get_size = il2cpp_symbols::get_method_pointer<int(*)(void*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "get_FontSize", 0
		);

		text_set_size = il2cpp_symbols::get_method_pointer<void(*)(void*, int)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_FontSize", 1
		);

		text_get_linespacing = il2cpp_symbols::get_method_pointer<float(*)(void*)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "get_lineSpacing", 0
		);

		text_set_style = il2cpp_symbols::get_method_pointer<void(*)(void*, int)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_fontStyle", 1
		);

		text_set_linespacing = il2cpp_symbols::get_method_pointer<void(*)(void*, float)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_lineSpacing", 1
		);

		text_set_horizontalOverflow = il2cpp_symbols::get_method_pointer<void(*)(void*, int)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_horizontalOverflow", 1
		);

		text_set_verticalOverflow = il2cpp_symbols::get_method_pointer<void(*)(void*, int)>(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"Text", "set_verticalOverflow", 1
		);

		auto RenderFrameRate_ChangeFrameRate_addr = il2cpp_symbols::get_method_pointer(
			"Prism.Legacy.dll", "PRISM.Legacy.Render3D", "RenderFrameRate",
			"ChangeFrameRate", 1);

		auto SystemUtility_set_FrameRate_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.Kernel.dll", "ENTERPRISE", "SystemUtility",
			"set_FrameRate", 1);

		auto RenderManager_SetResolutionRate_addr = il2cpp_symbols::get_method_pointer(
			"Prism.Rendering.Runtime.dll", "PRISM.Rendering", "RenderManager",
			"SetResolutionRate", 2);

		auto RenderManager_GetResolutionRate_addr = il2cpp_symbols::get_method_pointer(
			"Prism.Rendering.Runtime.dll", "PRISM.Rendering", "RenderManager",
			"GetResolutionRate", 1);

		auto RenderManager_GetResolutionSize_addr = il2cpp_symbols::get_method_pointer(
			"Prism.Rendering.Runtime.dll", "PRISM.Rendering", "RenderManager",
			"GetResolutionSize", 2);

		auto set_anti_aliasing_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_antiAliasing(System.Int32)");

		auto set_shadowResolution_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_shadowResolution(UnityEngine.ShadowResolution)");

		auto set_anisotropicFiltering_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_anisotropicFiltering(UnityEngine.AnisotropicFiltering)");

		auto set_shadows_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_shadows(UnityEngine.ShadowQuality)");

		auto set_softVegetation_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_softVegetation(System.Boolean)");

		auto set_realtimeReflectionProbes_addr = il2cpp_resolve_icall("UnityEngine.QualitySettings::set_realtimeReflectionProbes(System.Boolean)");

		auto Light_set_shadowResolution_addr = il2cpp_resolve_icall("UnityEngine.Light::set_shadowResolution(UnityEngine.Light,UnityEngine.Rendering.LightShadowResolution)");

		load_from_file = il2cpp_symbols::get_method_pointer<Il2CppObject * (*)(Il2CppString * path, uint32_t crc, uint64_t offset)>(
			"UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle",
			"LoadFromFile", 3);

		Shader_PropertyToID = il2cpp_symbols::get_method_pointer<int (*)(Il2CppString*)>("UnityEngine.CoreModule.dll", "UnityEngine", "Shader", "PropertyToID", 1);

		Material_HasProperty = il2cpp_symbols::get_method_pointer<bool (*)(Il2CppObject*, int)>("UnityEngine.CoreModule.dll", "UnityEngine", "Material", "HasProperty", 1);

		auto CriMana_Player_SetFile_addr = il2cpp_symbols::get_method_pointer("CriMw.CriWare.Runtime.dll", "CriWare.CriMana", "Player", "SetFile", 3);

		auto Object_Internal_CloneSingleWithParent_addr = il2cpp_resolve_icall("UnityEngine.Object::Internal_CloneSingleWithParent()");

		auto CatalogDB_ctor_addr = il2cpp_symbols::get_method_pointer(
			"Limelight.Runtime.dll", "Limelight", "CatalogDB",
			".ctor", 2);

		auto TweenManager_Update_addr = il2cpp_symbols::get_method_pointer(
			"DOTween.dll", "DG.Tweening.Core", "TweenManager", "Update", 3
		);

		auto LocalizationManager_LoadAsync_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.Localization.dll", "ENTERPRISE.Localization", "LocalizationManager",
			"LoadAsync", 1);

		auto Global_SystemReset_addr = il2cpp_symbols::get_method_pointer(
			"PRISM.ResourceManagement.dll", "PRISM", "Global",
			"SystemReset", -1);

		auto TimeUtility_SafeFormatString_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.Kernel.dll", "ENTERPRISE", "TimeUtility",
			"SafeFormatString", 2);

		auto TimeUtility_ToExpiredTimeText_addr = il2cpp_symbols::get_method_pointer(
			"ENTERPRISE.Kernel.dll", "ENTERPRISE", "TimeUtility",
			"ToExpiredTimeText", 2);

		auto LiveMVPresenter_OnApplicationFocus_addr = il2cpp_symbols::get_method_pointer(
			"PRISM.Legacy.dll", "PRISM.Live", "LiveMVPresenter",
			"OnApplicationFocus", 1);

		auto load_scene_internal_addr = il2cpp_resolve_icall("UnityEngine.SceneManagement.SceneManager::LoadSceneAsyncNameIndexInternal_Injected(System.String,System.Int32,UnityEngine.SceneManagement.LoadSceneParameters&,System.bool)");

#pragma endregion

		// ADD_HOOK(TimeUtility_ToExpiredTimeText, "ENTERPRISE.TimeUtility::ToExpiredTimeText at %p\n");

		ADD_HOOK(LiveMVPresenter_OnApplicationFocus, "PRISM.Live.LiveMVPresenter::OnApplicationFocus at %p\n");

		ADD_HOOK(Global_SystemReset, "PRISM.Global::SystemReset at %p\n");

		ADD_HOOK(LocalizationManager_LoadAsync, "ENTERPRISE.Localization.LocalizationManager::LoadAsync at %p\n");

		DumpLocalization("localization_local.json");

		OverwriteLocalization();

		// ADD_HOOK(CatalogDB_ctor, "Limelight.CatalogDB::ctor at %p\n");

		ADD_HOOK(CriMana_Player_SetFile, "CriWare.CriMana.Player::SetFile at %p\n");

		ADD_HOOK(set_shadowResolution, "UnityEngine.QualitySettings.set_shadowResolution(ShadowResolution) at %p\n");

		ADD_HOOK(set_anisotropicFiltering, "UnityEngine.QualitySettings.set_anisotropicFiltering(UnityEngine.AnisotropicFiltering) at %p\n");

		ADD_HOOK(set_shadows, "UnityEngine.QualitySettings.set_shadows(UnityEngine.ShadowQuality) at %p\n");

		ADD_HOOK(set_softVegetation, "UnityEngine.QualitySettings.set_softVegetation(System.Boolean) at %p\n");

		ADD_HOOK(set_realtimeReflectionProbes, "UnityEngine.QualitySettings.set_realtimeReflectionProbes(System.Boolean) at %p\n");

		ADD_HOOK(Light_set_shadowResolution, "UnityEngine.Light.set_shadowResolution(UnityEngine.Rendering.LightShadowResolution) at %p\n");

		ADD_HOOK(get_preferred_width, "UnityEngine.TextGenerator::GetPreferredWidth at %p\n");

		// hook UnityEngine.TextGenerator::PopulateWithErrors to modify text
		ADD_HOOK(populate_with_errors, "UnityEngine.TextGenerator::PopulateWithErrors at %p\n");

		ADD_HOOK(UITextMeshProUGUI_get_text_RawImage_get_texture_Image_get_sprite, "ENTERPRISE.UI.UITextMeshProUGUI::get_text\nUnityEngine.UI.Image::get_sprite\nUnityEngine.UI.RawImage::get_texture at %p\n");
		ADD_HOOK(UITextMeshProUGUI_set_text, "ENTERPRISE.UI.UITextMeshProUGUI::set_text at %p\n");

		ADD_HOOK(TweenManager_Update, "DG.Tweening.Core.TweenManager::Update at %p\n");

		if (g_unlock_size)
		{
			ADD_HOOK(canvas_scaler_setres, "UnityEngine.UI.CanvasScaler::set_referenceResolution at %p\n");
		}

		if (g_replace_to_custom_font)
		{
			ADD_HOOK(on_populate, "UnityEngine.UI.Text::OnPopulateMesh at %p\n");
			ADD_HOOK(UITextMeshProUGUI_Awake, "ENTERPRISE.UI.UITextMeshProUGUI::Awake at %p\n");
			ADD_HOOK(TMP_Text_GetTextElement, "TMPro.TMP_Text::GetTextElement at %p\n");
		}

		if (g_max_fps > -1)
		{
			// break 30-40fps limit
			ADD_HOOK(RenderFrameRate_ChangeFrameRate, "PRISM.Legacy.Render3D.RenderFrameRate::ChangeFrameRate at %p\n");
			ADD_HOOK(SystemUtility_set_FrameRate, "ENTERPRISE.SystemUtility::set_FrameRate at %p\n");
			ADD_HOOK(RenderManager_SetResolutionRate, "PRISM.Rendering.RenderManager::SetResolutionRate at %p\n");
			ADD_HOOK(RenderManager_GetResolutionRate, "PRISM.Rendering.RenderManager::GetResolutionRate at %p\n");
			ADD_HOOK(set_fps, "UnityEngine.Application.set_targetFrameRate at %p\n");
		}

		if (g_resolution_3d_scale != 1.0f)
		{
			ADD_HOOK(RenderManager_GetResolutionSize, "PRISM.Rendering.RenderManager::GetResolutionSize at %p\n");
		}

		if (g_anti_aliasing != -1)
		{
			ADD_HOOK(set_anti_aliasing, "UnityEngine.QualitySettings.set_antiAliasing(int) at %p\n");
		}
	}

	void patch_after_criware()
	{
		auto set_resolution_addr = il2cpp_resolve_icall("UnityEngine.Screen::SetResolution_Injected()");

		auto assetbundle_LoadFromFile_addr = il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromFile_Internal(System.String,System.UInt32,System.UInt64)");

		auto AssetBundleRequest_GetResult_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundleRequest",
			"GetResult", 0);

		auto assetbundle_load_asset_addr = il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadAsset_Internal(System.String,System.Type)");

		auto ImageConversion_LoadImage_addr = il2cpp_resolve_icall("UnityEngine.ImageConversion::LoadImage()");

		auto assetbundle_unload_addr = il2cpp_symbols::get_method_pointer("UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "Unload", 1);

		auto resources_load_addr = il2cpp_resolve_icall("UnityEngine.ResourcesAPIInternal::Load()");

		auto Sprite_get_texture_addr = il2cpp_resolve_icall("UnityEngine.Sprite::get_texture(UnityEngine.Sprite)");

		auto Renderer_get_material_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_material", 0);

		auto Renderer_get_materials_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_materials", 0);

		auto Renderer_get_sharedMaterial_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_sharedMaterial", 0);

		auto Renderer_get_sharedMaterials_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_sharedMaterials", 0);

		auto Renderer_set_material_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_material", 1);

		auto Renderer_set_materials_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_materials", 1);

		auto Renderer_set_sharedMaterial_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_sharedMaterial", 1);

		auto Renderer_set_sharedMaterials_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_sharedMaterials", 1);

		auto Material_get_mainTexture_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Material", "get_mainTexture", 0);

		auto Material_set_mainTexture_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "Material", "set_mainTexture", 1);

		auto RawImage_get_mainTexture_addr = il2cpp_symbols::get_method_pointer("UnityEngine.UI.dll", "UnityEngine.UI", "RawImage", "get_mainTexture", 0);

		auto Material_SetTextureI4_addr = il2cpp_symbols::find_method("UnityEngine.CoreModule.dll", "UnityEngine", "Material", [](const MethodInfo* method)
			{
				return method->name == "SetTexture"s &&
					method->parameters[0].parameter_type->type == IL2CPP_TYPE_I4;
			});

		auto Material_GetTextureImpl_addr = il2cpp_resolve_icall("UnityEngine.Material::GetTextureImpl(System.String,System.Int32)");

		auto Material_SetTextureImpl_addr = il2cpp_resolve_icall("UnityEngine.Material::SetTextureImpl(System.String,System.Int32,UnityEngine.Texture)");

		auto GameObject_GetComponent_addr = il2cpp_resolve_icall("UnityEngine.GameObject::GetComponent(System.Type)");

		auto GameObject_GetComponentFastPath_addr = il2cpp_resolve_icall("UnityEngine.GameObject::GetComponentFastPath(System.Type,System.IntPtr)");

#pragma region LOAD_ASSETBUNDLE
		if (!fontAssets && !g_font_assetbundle_path.empty() && g_replace_to_custom_font)
		{
			auto assetbundlePath = local::u8_wide(g_font_assetbundle_path);
			if (PathIsRelativeW(assetbundlePath.data()))
			{
				assetbundlePath.insert(0, ((wstring)filesystem::current_path().native()).append(L"/"));
			}

			cout << "Loading asset: " << local::wide_u8(assetbundlePath) << endl;

			fontAssets = load_from_file(il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()), 0, 0);

			if (!fontAssets && filesystem::exists(assetbundlePath))
			{
				cout << "Asset founded but not loaded. Maybe Asset BuildTarget is not for Windows\n";
			}
		}

		if (fontAssets)
		{
			cout << "Asset loaded: " << fontAssets << endl;
		}

		if (!g_replace_assetbundle_file_path.empty())
		{
			auto assetbundlePath = local::u8_wide(g_replace_assetbundle_file_path);
			if (PathIsRelativeW(assetbundlePath.data()))
			{
				assetbundlePath.insert(0, ((wstring)filesystem::current_path().native()).append(L"/"));
			}

			cout << "Loading replacement AssetBundle: " << local::wide_u8(assetbundlePath) << endl;

			auto assets = load_from_file(il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()), 0, 0);

			if (!assets && filesystem::exists(assetbundlePath))
			{
				cout << "Replacement AssetBundle founded but not loaded. Maybe Asset BuildTarget is not for Windows\n";
			}
			else
			{
				cout << "Replacement AssetBundle loaded: " << assets << endl;
				replaceAssets.emplace_back(assets);
			}
		}

		if (!g_replace_assetbundle_file_paths.empty())
		{
			for (auto it = g_replace_assetbundle_file_paths.begin(); it != g_replace_assetbundle_file_paths.end(); it++)
			{

				auto assetbundlePath = local::u8_wide(*it);
				if (PathIsRelativeW(assetbundlePath.data()))
				{
					assetbundlePath.insert(0, ((wstring)filesystem::current_path().native()).append(L"/"));
				}

				cout << "Loading replacement AssetBundle: " << local::wide_u8(assetbundlePath) << endl;

				auto assets = load_from_file(il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()), 0, 0);

				if (!assets && filesystem::exists(assetbundlePath))
				{
					cout << "Replacement AssetBundle founded but not loaded. Maybe Asset BuildTarget is not for Windows\n";
				}
				else if (assets)
				{
					cout << "Replacement AssetBundle loaded: " << assets << "\n";
					replaceAssets.emplace_back(assets);
				}
			}
		}

		if (!replaceAssets.empty())
		{
			for (auto it = replaceAssets.begin(); it != replaceAssets.end(); it++)
			{
				auto names = get_all_asset_names(*it);
				for (int i = 0; i < names->max_length; i++)
				{
					auto u8Name = local::wide_u8(static_cast<Il2CppString*>(names->vector[i])->start_char);
					replaceAssetNames.emplace_back(u8Name);
				}
			}
		}
#pragma endregion

		ADD_HOOK(AssetBundleRequest_GetResult, "UnityEngine.AssetBundleRequest::GetResult at %p\n");

		ADD_HOOK(assetbundle_LoadFromFile, "UnityEngine.AssetBundle::LoadFromFile at %p\n");

		ADD_HOOK(assetbundle_unload, "UnityEngine.AssetBundle::Unload at %p\n");

		ADD_HOOK(resources_load, "UnityEngine.Resources::Load at %p\n");

		if (g_dump_texture || !replaceAssets.empty())
		{
			ADD_HOOK(assetbundle_load_asset, "UnityEngine.AssetBundle::LoadAsset at %p\n");

			ADD_HOOK(ImageConversion_LoadImage, "UnityEngine.ImageConversion::LoadImage at %p\n");

			ADD_HOOK(GameObject_GetComponent, "UnityEngine.GameObject::GetComponent at %p\n");

			// ADD_HOOK(GameObject_GetComponentFastPath, "UnityEngine.GameObject::GetComponentFastPath at %p\n");

			ADD_HOOK(Sprite_get_texture, "UnityEngine.Sprite::get_texture at %p\n");

			ADD_HOOK(Renderer_get_material, "UnityEngine.Renderer::get_material at %p\n");

			ADD_HOOK(Renderer_get_materials, "UnityEngine.Renderer::get_materials at %p\n");

			ADD_HOOK(Renderer_get_sharedMaterial, "UnityEngine.Renderer::get_sharedMaterial at %p\n");

			ADD_HOOK(Renderer_get_sharedMaterials, "UnityEngine.Renderer::get_sharedMaterials at %p\n");

			ADD_HOOK(Renderer_set_material, "UnityEngine.Renderer::set_material at %p\n");

			ADD_HOOK(Renderer_set_materials, "UnityEngine.Renderer::set_materials at %p\n");

			ADD_HOOK(Renderer_set_sharedMaterial, "UnityEngine.Renderer::set_sharedMaterial at %p\n");

			ADD_HOOK(Renderer_set_sharedMaterials, "UnityEngine.Renderer::set_sharedMaterials at %p\n");

			ADD_HOOK(Material_get_mainTexture, "UnityEngine.Material::get_mainTexture at %p\n");

			ADD_HOOK(Material_set_mainTexture, "UnityEngine.Material::set_mainTexture at %p\n");

			ADD_HOOK(RawImage_get_mainTexture, "UnityEngine.UI.RawImage::get_mainTexture at %p\n");

			ADD_HOOK(Material_SetTextureI4, "UnityEngine.Material::SetTexture at %p\n");

			ADD_HOOK(Material_GetTextureImpl, "UnityEngine.Material::GetTextureImpl at %p\n");

			ADD_HOOK(Material_SetTextureImpl, "UnityEngine.Material::SetTextureImpl at %p\n");
		}

		if (g_dump_entries)
		{
			dump_all_entries();
		}

		if (g_unlock_size)
		{
			ADD_HOOK(set_resolution, "UnityEngine.Screen.SetResolution(int, int, bool, RefreshRate) at %p\n");

			auto hWnd = GetHWND();

			long style = GetWindowLongW(hWnd, GWL_STYLE);
			style |= WS_MAXIMIZEBOX;
			style |= WS_THICKFRAME;
			SetWindowLongPtrW(hWnd, GWL_STYLE, style);

			oldWndProcPtr = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndproc_hook)));
		}
	}
}

bool init_hook_base()
{
	if (mh_inited)
		return false;

	if (MH_Initialize() != MH_OK)
		return false;

	mh_inited = true;

	MH_CreateHook(LoadLibraryW, load_library_w_hook, &load_library_w_orig);
	MH_EnableHook(LoadLibraryW);

	auto UnityPlayer = GetModuleHandle("UnityPlayer.dll");
	auto UnityMain_addr = GetProcAddress(UnityPlayer, "UnityMain");

	MH_CreateHook(UnityMain_addr, UnityMain_hook, &UnityMain_orig);
	MH_EnableHook(UnityMain_addr);

	return true;
}

bool init_hook()
{
	return true;
}

void uninit_hook()
{
	if (!mh_inited)
		return;

	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
