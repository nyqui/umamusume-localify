#include <stdinclude.hpp>
#include <set>

using namespace std;

namespace
{
	void path_game_assembly();

	bool mh_inited = false;

	void dump_bytes(void* pos)
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

		printf("\n\n");
	}

	void* load_library_w_orig = nullptr;
	HMODULE __stdcall load_library_w_hook(const wchar_t* path)
	{
		// GameAssembly.dll code must be loaded and decrypted while loading criware library
		if (path == L"cri_ware_unity.dll"s)
		{
			path_game_assembly();

			MH_DisableHook(LoadLibraryW);
			MH_RemoveHook(LoadLibraryW);

			if (!g_custom_title_name.empty()) {
				SetWindowText(GetActiveWindow(), local::wide_acp(local::u8_wide(g_custom_title_name)).data());
			}

			// use original function beacuse we have unhooked that
			return LoadLibraryW(path);
		}

		return reinterpret_cast<decltype(LoadLibraryW)*>(load_library_w_orig)(path);
	}

	Il2CppObject* assets = nullptr;

	Il2CppObject* uiManager = nullptr;

	Il2CppObject* (*load_from_file)(Il2CppString* path);

	Il2CppObject* (*load_assets)(Il2CppObject* _this, Il2CppString* name, Il2CppObject* runtimeType);

	Il2CppString* (*uobject_get_name)(Il2CppObject* uObject);

	Il2CppString* (*get_unityVersion)();

	void PrintStackTrace()
	{
		Il2CppString* (*trace)() = reinterpret_cast<Il2CppString * (*)()>(il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "StackTraceUtility", "ExtractStackTrace", 0));
		cout << local::wide_u8(wstring(trace()->start_char)) << "\n";
	}

	Il2CppObject* GetRuntimeType(const char* assemblyName, const char* namespaze, const char* klassName)
	{
		return il2cpp_type_get_object(il2cpp_class_get_type(il2cpp_symbols::get_class(assemblyName, namespaze, klassName)));
	}

	Boolean GetBoolean(bool value)
	{
		return reinterpret_cast<Boolean(*)(Il2CppString * value)>(il2cpp_symbols::get_method_pointer(
			"mscorlib.dll", "System", "Boolean", "Parse", 1))(
				il2cpp_string_new(value ? "true" : "false"));
	}

	Il2CppObject* GetCustomFont() {
		if (!assets) return nullptr;
		return load_assets(assets, il2cpp_string_new(g_font_asset_name.data()), GetRuntimeType("UnityEngine.TextRenderingModule.dll", "UnityEngine", "Font"));
	}

	string GetUnityVersion() {
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

	// UnityEngine.UI.dll UnityEngine.UI Text
	void* text_get_text_orig = nullptr;
	Il2CppString* text_get_text_hook(Il2CppObject* _this)
	{
		return local::get_localized_string(reinterpret_cast<decltype(text_get_text_hook)*>(text_get_text_orig) (_this));
	}

	void* localizeextension_text_orig = nullptr;
	Il2CppString* localizeextension_text_hook(int id)
	{
		Il2CppString* localized = local::get_localized_string(id);
		return localized ? localized
			: reinterpret_cast<decltype(localizeextension_text_hook)*>(localizeextension_text_orig)(
				id
				);
	}

	void ReplaceTextMeshFont(Il2CppObject* textMesh, Il2CppObject* meshRenderer) {
		Il2CppObject* font = GetCustomFont();
		Il2CppObject* fontMaterial = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(font->klass, "get_material", 0)->methodPointer)(font);
		Il2CppObject* fontTexture = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(fontMaterial->klass, "get_mainTexture", 0)->methodPointer)(fontMaterial);

		reinterpret_cast<void (*)(Il2CppObject * _this, Il2CppObject * font)>(il2cpp_class_get_method_from_name(textMesh->klass, "set_font", 1)->methodPointer)(textMesh, font);
		if (meshRenderer)
		{
			auto get_sharedMaterial = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(meshRenderer->klass, "GetSharedMaterial", 0)->methodPointer);

			Il2CppObject* sharedMaterial = get_sharedMaterial(meshRenderer);
			reinterpret_cast<void (*)(Il2CppObject * _this, Il2CppObject * texture)>(il2cpp_class_get_method_from_name(sharedMaterial->klass, "set_mainTexture", 1)->methodPointer)(sharedMaterial, fontTexture);
		}
	}

	void* get_preferred_width_orig = nullptr;
	float get_preferred_width_hook(void* _this, Il2CppString* str, TextGenerationSettings_t* settings)
	{
		return reinterpret_cast<decltype(get_preferred_width_hook)*>(get_preferred_width_orig) (
			_this, local::get_localized_string(str), settings
			);
	}

	void* localize_get_orig = nullptr;
	Il2CppString* localize_get_hook(int id)
	{
		auto orig_result = reinterpret_cast<decltype(localize_get_hook)*>(localize_get_orig)(id);
		auto result = local::get_localized_string(id);

		return result ? result : orig_result;
	}

	void* an_text_set_material_to_textmesh_orig = nullptr;
	void an_text_set_material_to_textmesh_hook(Il2CppObject* _this)
	{
		reinterpret_cast<decltype(an_text_set_material_to_textmesh_hook)*>(an_text_set_material_to_textmesh_orig)(_this);
		if (!(assets && g_replace_to_custom_font)) return;

		FieldInfo* mainField = il2cpp_class_get_field_from_name(_this->klass, "_mainTextMesh");
		FieldInfo* mainRenderer = il2cpp_class_get_field_from_name(_this->klass, "_mainTextMeshRenderer");

		FieldInfo* outlineField = il2cpp_class_get_field_from_name(_this->klass, "_outlineTextMeshList"); //List<TextMesh>
		FieldInfo* outlineFieldRenderers = il2cpp_class_get_field_from_name(_this->klass, "_outlineTextMeshRendererList"); //List<MeshRenderer>

		FieldInfo* shadowField = il2cpp_class_get_field_from_name(_this->klass, "_shadowTextMesh");
		FieldInfo* shadowFieldRenderer = il2cpp_class_get_field_from_name(_this->klass, "_shadowTextMeshRenderer");

		Il2CppObject* mainMesh;
		Il2CppObject* mainMeshRenderer;

		Il2CppObject* outlineMeshes;
		Il2CppObject* outlineMeshRenderers;

		Il2CppObject* shadowMesh;
		Il2CppObject* shadowMeshRenderer;

		il2cpp_field_get_value(_this, mainField, &mainMesh);
		il2cpp_field_get_value(_this, mainRenderer, &mainMeshRenderer);

		ReplaceTextMeshFont(mainMesh, mainMeshRenderer);

		vector<Il2CppObject*> textMeshies;
		il2cpp_field_get_value(_this, outlineField, &outlineMeshes);
		if (outlineMeshes)
		{
			FieldInfo* itemsField = il2cpp_class_get_field_from_name(outlineMeshes->klass, "_items");
			Il2CppArraySize* arr;
			il2cpp_field_get_value(outlineMeshes, itemsField, &arr);
			if (arr)
			{
				for (int i = 0; i < arr->max_length; i++)
				{
					Il2CppObject* mesh = reinterpret_cast<Il2CppObject*>(arr->vector[i]);
					if (!mesh)
					{
						break;
					}
					textMeshies.push_back(mesh);
				}
			}
		}

		il2cpp_field_get_value(_this, outlineFieldRenderers, &outlineMeshRenderers);
		if (outlineMeshRenderers)
		{
			FieldInfo* itemsField = il2cpp_class_get_field_from_name(outlineMeshRenderers->klass, "_items");
			Il2CppArraySize* arr;
			il2cpp_field_get_value(outlineMeshRenderers, itemsField, &arr);
			if (arr)
			{
				for (int i = 0; i < textMeshies.size(); i++)
				{
					Il2CppObject* meshRenderer = reinterpret_cast<Il2CppObject*>(arr->vector[i]);
					ReplaceTextMeshFont(textMeshies[i], meshRenderer);
				}
			}
		}

		il2cpp_field_get_value(_this, shadowField, &shadowMesh);
		if (shadowMesh)
		{
			il2cpp_field_get_value(_this, shadowFieldRenderer, &shadowMeshRenderer);
			ReplaceTextMeshFont(shadowMesh, shadowMeshRenderer);
		}
	}

	void* an_text_fix_data_orig = nullptr;
	void an_text_fix_data_hook(Il2CppObject* _this)
	{
		reinterpret_cast<decltype(an_text_fix_data_hook)*>(an_text_fix_data_orig)(_this);
		FieldInfo* field = il2cpp_class_get_field_from_name(_this->klass, "_text");
		Il2CppString* str;
		il2cpp_field_get_value(_this, field, &str);
		il2cpp_field_set_value(_this, field, local::get_localized_string(str));
	}

	void* text_clip_data_ctor_orig = nullptr;
	void text_clip_data_ctor_hook(Il2CppObject* _this)
	{
		// cout << "FinishTypewrite\n";
		// Il2CppString* (*trace)() = reinterpret_cast<Il2CppString * (*)()>(il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine", "StackTraceUtility", "ExtractStackTrace", 0));
		// cout << local::wide_u8(wstring(trace()->start_char)) << "\n";

		//auto field = il2cpp_class_get_field_from_name(il2cpp_symbols::get_class("umamusume.dll", "Gallop", "StoryTimelineData"), "TypewriteCountPerSecond");
		//cout << field->name << "\n";
		//cout << (field->type->type == IL2CPP_TYPE_I4) << "\n";
		//// il2cpp_object_new(il2cpp_symbols::get_class("mscorlib.dll", "System", "Int32"));

		//int value = 20;
		//il2cpp_field_set_value(_this, field, &value);
		//int result = 0;
		//il2cpp_field_get_value(_this, field, &result);
		//printf("%d\n", result);
		return;
	}

	/*void* get_time_from_frame_orig = nullptr;
	float get_time_from_frame_hook(Il2CppObject* _this, int frame, float fps) {
		return reinterpret_cast<decltype(get_time_from_frame_hook)*>(get_time_from_frame_orig)(_this, frame, 144);
	}*/

	void* update_orig = nullptr;
	void* update_hook(Il2CppObject* _this, void* updateType, float deltaTime, float independentTime)
	{
		return reinterpret_cast<decltype(update_hook)*>(update_orig)(_this, updateType, deltaTime * g_ui_animation_scale, independentTime * g_ui_animation_scale);
	}

	/*void* timeline_audioclip_ctor_orig = nullptr;
	void* timeline_audioclip_ctor_hook(Il2CppObject* _this, int frameCount, float currentTime, bool isWaiting) {
		cout << "frameCount: " << frameCount << "\n";
		cout << "currentTime: " << currentTime << "\n";
		cout << "isWaiting: " << isWaiting << "\n";
		return reinterpret_cast<decltype(timeline_audioclip_ctor_hook)*>(timeline_audioclip_ctor_orig)(_this, round(currentTime * 144), currentTime, isWaiting);
	}*/

	void* get_start_frame_orig = nullptr;
	int get_start_frame_hook(Il2CppObject* _this)
	{
		auto orig_result = reinterpret_cast<decltype(get_start_frame_hook)*>(get_start_frame_orig)(_this);
		if (orig_result == 0) return 0;
		// cout << "get_StartFrame: " << orig_result << "\n";
		if (orig_result < 100) {
			return orig_result;
		}
		return round(orig_result);
	}

	void* get_end_frame_orig = nullptr;
	int get_end_frame_hook(Il2CppObject* _this)
	{
		auto orig_result = reinterpret_cast<decltype(get_end_frame_hook)*>(get_end_frame_orig)(_this);
		// cout << "get_EndFrame: " << orig_result << "\n";
		return round(orig_result);
	}

	void* get_end_frame1_orig = nullptr;
	int get_end_frame1_hook(Il2CppObject* _this)
	{
		auto orig_result = reinterpret_cast<decltype(get_end_frame1_hook)*>(get_end_frame1_orig)(_this);
		// cout << "get_EndFrame1: " << orig_result << "\n";
		return round(orig_result);
	}

	void* update_timeline_data_orig = nullptr;
	void* update_timeline_data_hook(Il2CppObject* _this, int frameCount, float currentTime, bool isWaiting, bool isChangeBlock)
	{
		return reinterpret_cast<decltype(update_timeline_data_hook)*>(update_timeline_data_orig)(_this, round(currentTime * 144), currentTime, isWaiting, isChangeBlock);
	}

	void* get_fixed_start_frame_orig = nullptr;
	int get_fixed_start_frame_hook(Il2CppObject* _this)
	{
		auto orig_result = reinterpret_cast<decltype(get_fixed_start_frame_hook)*>(get_fixed_start_frame_orig)(_this);
		return orig_result;
	}

	void* get_fixed_end_frame_orig = nullptr;
	int get_fixed_end_frame_hook(Il2CppObject* _this)
	{
		auto orig_result = reinterpret_cast<decltype(get_fixed_end_frame_hook)*>(get_fixed_end_frame_orig)(_this);
		return round(orig_result);
	}

	std::unordered_map<void*, bool> text_queries;

	void* query_ctor_orig = nullptr;
	void* query_ctor_hook(void* _this, void* conn, Il2CppString* sql)
	{
		auto ssql = std::wstring(sql->start_char);

		if (ssql.find(L"text_data") != std::string::npos ||
			ssql.find(L"character_system_text") != std::string::npos ||
			ssql.find(L"race_jikkyo_comment") != std::string::npos ||
			ssql.find(L"race_jikkyo_message") != std::string::npos)
		{
			text_queries.emplace(_this, true);
		}

		return reinterpret_cast<decltype(query_ctor_hook)*>(query_ctor_orig)(_this, conn, sql);
	}

	void* query_dispose_orig = nullptr;
	void query_dispose_hook(void* _this)
	{
		if (text_queries.contains(_this))
			text_queries.erase(_this);

		return reinterpret_cast<decltype(query_dispose_hook)*>(query_dispose_orig)(_this);
	}

	void* query_getstr_orig = nullptr;
	Il2CppString* query_getstr_hook(void* _this, int idx)
	{
		auto result = reinterpret_cast<decltype(query_getstr_hook)*>(query_getstr_orig)(_this, idx);

		if (text_queries.contains(_this))
			return local::get_localized_string(result);

		return result;
	}

	void* story_timeline_controller_play_orig;
	void* story_timeline_controller_play_hook(Il2CppObject* _this)
	{
		FieldInfo* timelineDataField = il2cpp_class_get_field_from_name(_this->klass, "_timelineData");

		Il2CppObject* timelineData;
		il2cpp_field_get_value(_this, timelineDataField, &timelineData);
		FieldInfo* StoryTimelineDataClass_TitleField = il2cpp_class_get_field_from_name(timelineData->klass, "Title");
		FieldInfo* StoryTimelineDataClass_BlockListField = il2cpp_class_get_field_from_name(timelineData->klass, "BlockList");

		Il2CppClass* story_timeline_text_clip_data_class = il2cpp_symbols::get_class("umamusume.dll", "Gallop", "StoryTimelineTextClipData");
		FieldInfo* nameField = il2cpp_class_get_field_from_name(story_timeline_text_clip_data_class, "Name");
		FieldInfo* textField = il2cpp_class_get_field_from_name(story_timeline_text_clip_data_class, "Text");
		FieldInfo* choiceDataListField = il2cpp_class_get_field_from_name(story_timeline_text_clip_data_class, "ChoiceDataList");
		FieldInfo* colorTextInfoListField = il2cpp_class_get_field_from_name(story_timeline_text_clip_data_class, "ColorTextInfoList");

		Il2CppString* title;
		il2cpp_field_get_value(timelineData, StoryTimelineDataClass_TitleField, &title);
		il2cpp_field_set_value(timelineData, StoryTimelineDataClass_TitleField, local::get_localized_string(title));

		Il2CppObject* blockList;
		il2cpp_field_get_value(timelineData, StoryTimelineDataClass_BlockListField, &blockList);

		Il2CppArraySize* blockArray;
		il2cpp_field_get_value(blockList, il2cpp_class_get_field_from_name(blockList->klass, "_items"), &blockArray);

		for (size_t i = 0; i < blockArray->max_length; i++)
		{
			Il2CppObject* blockData = reinterpret_cast<Il2CppObject*>(blockArray->vector[i]);
			if (!blockData) break;
			FieldInfo* StoryTimelineBlockDataClass_trackListField = il2cpp_class_get_field_from_name(blockData->klass, "_trackList");
			Il2CppObject* trackList;
			il2cpp_field_get_value(blockData, StoryTimelineBlockDataClass_trackListField, &trackList);

			Il2CppArraySize* trackArray;
			il2cpp_field_get_value(trackList, il2cpp_class_get_field_from_name(trackList->klass, "_items"), &trackArray);

			for (size_t i = 0; i < trackArray->max_length; i++)
			{
				Il2CppObject* trackData = reinterpret_cast<Il2CppObject*>(trackArray->vector[i]);
				if (!trackData) break;
				FieldInfo* StoryTimelineTrackDataClass_ClipListField = il2cpp_class_get_field_from_name(trackData->klass, "ClipList");
				Il2CppObject* clipList;
				il2cpp_field_get_value(trackData, StoryTimelineTrackDataClass_ClipListField, &clipList);


				Il2CppArraySize* clipArray;
				il2cpp_field_get_value(clipList, il2cpp_class_get_field_from_name(clipList->klass, "_items"), &clipArray);

				for (size_t i = 0; i < clipArray->max_length; i++)
				{
					Il2CppObject* clipData = reinterpret_cast<Il2CppObject*>(clipArray->vector[i]);
					if (!clipData) break;
					if (story_timeline_text_clip_data_class == clipData->klass)
					{
						Il2CppString* name;
						il2cpp_field_get_value(clipData, nameField, &name);
						il2cpp_field_set_value(clipData, nameField, local::get_localized_string(name));
						Il2CppString* text;
						il2cpp_field_get_value(clipData, textField, &text);
						il2cpp_field_set_value(clipData, textField, local::get_localized_string(text));
						Il2CppObject* choiceDataList;
						il2cpp_field_get_value(clipData, choiceDataListField, &choiceDataList);
						if (choiceDataList)
						{
							Il2CppArraySize* choiceDataArray;
							il2cpp_field_get_value(choiceDataList, il2cpp_class_get_field_from_name(choiceDataList->klass, "_items"), &choiceDataArray);

							for (size_t i = 0; i < choiceDataArray->max_length; i++)
							{
								Il2CppObject* choiceData = reinterpret_cast<Il2CppObject*>(choiceDataArray->vector[i]);
								if (!choiceData) break;
								FieldInfo* choiceDataTextField = il2cpp_class_get_field_from_name(choiceData->klass, "Text");

								Il2CppString* text;
								il2cpp_field_get_value(choiceData, choiceDataTextField, &text);
								il2cpp_field_set_value(choiceData, choiceDataTextField, local::get_localized_string(text));
							}
						}
						Il2CppObject* colorTextInfoList;
						il2cpp_field_get_value(clipData, colorTextInfoListField, &colorTextInfoList);
						if (colorTextInfoList)
						{
							Il2CppArraySize* colorTextInfoArray;
							il2cpp_field_get_value(colorTextInfoList, il2cpp_class_get_field_from_name(colorTextInfoList->klass, "_items"), &colorTextInfoArray);

							for (size_t i = 0; i < colorTextInfoArray->max_length; i++)
							{
								Il2CppObject* colorTextInfo = reinterpret_cast<Il2CppObject*>(colorTextInfoArray->vector[i]);
								if (!colorTextInfo) break;
								FieldInfo* colorTextInfoTextField = il2cpp_class_get_field_from_name(colorTextInfo->klass, "Text");

								Il2CppString* text;
								il2cpp_field_get_value(colorTextInfo, colorTextInfoTextField, &text);
								il2cpp_field_set_value(colorTextInfo, colorTextInfoTextField, local::get_localized_string(text));
							}
						}
					}

				}
			}
		}

		return reinterpret_cast<decltype(story_timeline_controller_play_hook)*>(story_timeline_controller_play_orig)(_this);
	}

	void* story_race_textasset_load_orig;
	void story_race_textasset_load_hook(Il2CppObject* _this)
	{
		FieldInfo* textDataField = { il2cpp_class_get_field_from_name(_this->klass, "textData") };
		Il2CppObject* textData;
		il2cpp_field_get_value(_this, textDataField, &textData);

		auto enumerator = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(textData->klass, "GetEnumerator", 0)->methodPointer)(textData);
		auto get_current = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(enumerator->klass, "get_Current", 0)->methodPointer);
		auto move_next = reinterpret_cast<bool (*)(Il2CppObject * _this)>(il2cpp_class_get_method_from_name(enumerator->klass, "MoveNext", 0)->methodPointer);

		while (move_next(enumerator))
		{
			auto key = get_current(enumerator);
			FieldInfo* textField = { il2cpp_class_get_field_from_name(key->klass, "text") };
			Il2CppString* text;
			il2cpp_field_get_value(key, textField, &text);
			il2cpp_field_set_value(key, textField, local::get_localized_string(text));
		}

		reinterpret_cast<decltype(story_race_textasset_load_hook)*>(story_race_textasset_load_orig)(_this);
	}

	bool useDefaultFPS = false;

	void* set_fps_orig = nullptr;
	void set_fps_hook(int value)
	{
		return reinterpret_cast<decltype(set_fps_hook)*>(set_fps_orig)(useDefaultFPS ? value : g_max_fps);
	}

	bool (*is_virt)() = nullptr;

	Il2CppObject* (*display_get_main)();

	int (*get_system_width)(Il2CppObject* thisObj);

	int (*get_system_height)(Il2CppObject* thisObj);

	int (*get_rendering_width)(Il2CppObject* thisObj);

	int (*get_rendering_height)(Il2CppObject* thisObj);

	int last_display_width = 0, last_display_height = 0;
	int last_virt_window_width = 0, last_virt_window_height = 0;
	int last_hriz_window_width = 0, last_hriz_window_height = 0;

	bool fullScreenFl = g_force_landscape && g_auto_fullscreen;
	bool fullScreenFlOverride = false;

	Resolution_t* (*get_resolution)(Resolution_t* buffer);

	void get_resolution_stub(Resolution_t* r)
	{
		*r = *get_resolution(r);

		int width = min(r->height, r->width) * g_aspect_ratio;
		if (r->width > r->height)
			r->width = width;
		else
			r->height = width;
	}

	void* set_resolution_orig;
	void set_resolution_hook(int width, int height, bool fullscreen)
	{
		Resolution_t r;
		r = *get_resolution(&r);

		if (g_force_landscape && !g_auto_fullscreen)
		{
			fullScreenFl = false;
			if (width < height)
			{
				return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(height, width, false);
			}
			return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(width, height, false);
		}
		bool reqVirt = width < height;

		if (is_virt() && fullScreenFl)
		{
			fullScreenFl = false;
			fullScreenFlOverride = false;
			return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(last_virt_window_width, last_virt_window_height, false);
		}

		auto display = display_get_main();

		if (reqVirt && (get_rendering_width(display) > get_rendering_height(display)))
		{
			fullScreenFl = false;
			fullScreenFlOverride = false;
			if (last_virt_window_width > last_virt_window_height)
			{
				return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(last_virt_window_height, last_virt_window_width, false);
			}
			return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(last_virt_window_width, last_virt_window_height, false);
		}

		bool need_fullscreen = false;

		if (g_auto_fullscreen) {
			if (is_virt() && r.width / static_cast<double>(r.height) == (9.0 / 16.0))
				need_fullscreen = true;
			else if (!is_virt() && r.width / static_cast<double>(r.height) == (16.0 / 9.0))
				need_fullscreen = true;
		}

		if (!fullScreenFl && !g_force_landscape)
		{
			if (!(get_rendering_width(display) > get_rendering_height(display)))
			{
				last_virt_window_width = get_rendering_width(display);
				last_virt_window_height = get_rendering_height(display);
				if (need_fullscreen && (!last_hriz_window_width || !last_hriz_window_height)) {
					float new_ratio = static_cast<float>(r.width) / r.height;

					last_hriz_window_width = r.width - 400;
					last_hriz_window_height = last_hriz_window_width / new_ratio;
				}
			}
			else
			{
				last_hriz_window_width = get_rendering_width(display);
				last_hriz_window_height = get_rendering_height(display);
			}
		}

		if (!fullScreenFlOverride) {
			fullScreenFl = need_fullscreen;
		}

		if (!reqVirt && !fullScreenFl && last_hriz_window_width && last_hriz_window_height)
		{
			width = last_hriz_window_width;
			height = last_hriz_window_height;
		}

		return reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(
			need_fullscreen ? r.width : width, need_fullscreen ? r.height : height, fullScreenFl
			);
	}

	void* get_virt_size_orig = nullptr;
	Vector3_t* get_virt_size_hook(Vector3_t* pVec3, int width, int height)
	{
		auto size = reinterpret_cast<decltype(get_virt_size_hook)*>(get_virt_size_orig)(pVec3, width, height);

		height = width * g_aspect_ratio;

		size->x = width;
		size->y = height;
		size->z = g_aspect_ratio;

		return size;
	}

	void* get_hori_size_orig = nullptr;
	Vector3_t* get_hori_size_hook(Vector3_t* pVec3, int width, int height)
	{
		auto size = reinterpret_cast<decltype(get_hori_size_hook)*>(get_hori_size_orig)(pVec3, width, height);

		width = height * g_aspect_ratio;

		size->x = width;
		size->y = height;
		size->z = g_aspect_ratio;

		return size;
	}

	void* gallop_get_screenheight_orig;
	int gallop_get_screenheight_hook()
	{
		int w = max(last_display_width, last_display_height), h = min(last_display_width, last_display_height);

		return is_virt() ? w : h;
	}

	void* gallop_get_screenwidth_orig;
	int gallop_get_screenwidth_hook()
	{
		int w = max(last_display_width, last_display_height), h = min(last_display_width, last_display_height);

		return is_virt() ? h : w;
	}

	void (*set_scale_factor)(void*, float);

	void* canvas_scaler_setres_orig;
	void canvas_scaler_setres_hook(Il2CppObject* _this, Vector2_t res)
	{
		Resolution_t r;
		get_resolution_stub(&r);

		if (g_force_landscape)
		{
			res.x /= (max(1.0f, r.width / 1920.f) * g_force_landscape_ui_scale);
			res.y /= (max(1.0f, r.height / 1080.f) * g_force_landscape_ui_scale);
		}
		else
		{
			res.x = r.width;
			res.y = r.height;
		}

		// set scale factor to make ui bigger on hi-res screen
		set_scale_factor(_this, max(1.0f, r.width / 1920.f) * g_force_landscape ? g_force_landscape_ui_scale : g_ui_scale);

		return reinterpret_cast<decltype(canvas_scaler_setres_hook)*>(canvas_scaler_setres_orig)(_this, res);
	}

	void* UIManager_ChangeResizeUIForPC_orig = nullptr;
	void UIManager_ChangeResizeUIForPC_hook(Il2CppObject* _this, int width, int height) {
		uiManager = _this;
		reinterpret_cast<decltype(UIManager_ChangeResizeUIForPC_hook)*>(UIManager_ChangeResizeUIForPC_orig)(_this, width, height);
	}

	void* GetLimitSize_orig = nullptr;
	Vector2_t GetLimitSize_hook() {
		auto orig = reinterpret_cast<decltype(GetLimitSize_hook)*>(GetLimitSize_orig)();
		Resolution_t r;
		get_resolution_stub(&r);
		orig.x = r.width;
		orig.y = r.height;
		return orig;
	}

	bool altEnterPressed = false;

	void* wndproc_orig = nullptr;
	LRESULT wndproc_hook(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		//if (uMsg == WM_MOVING)
		//{
		//	auto currentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		//	MONITORINFO monitor;
		//	monitor.cbSize = sizeof(MONITORINFO);
		//	GetMonitorInfoW(currentMonitor, &monitor);
		//	float height = monitor.rcMonitor.bottom - monitor.rcMonitor.top;
		//	float width = monitor.rcMonitor.right - monitor.rcMonitor.left;
		//	if (last_display_width != width || last_display_height != height) {
		//		cout << "monitor: " << " " << width << " " << height << "\n";
		//		last_display_width = width;
		//		last_display_height = height;
		//		reinterpret_cast<void (*)(int)>(il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "Screen", "set_OriginalScreenWidth", 1))(width);
		//		reinterpret_cast<void (*)(int)>(il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "Screen", "set_OriginalScreenHeight", 1))(height);
		//		if (uiManager) {
		//			// Gallop.UIManager:ChangeResizeUIForPC
		//			reinterpret_cast<Boolean(*)()>(il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "Screen", "UpdateForPC", -1))();
		//			reinterpret_cast<void (*)(Il2CppObject*, int, int)>(il2cpp_class_get_method_from_name(uiManager->klass, "ChangeResizeUIForPC", 2)->methodPointer)(uiManager, width, height);
		//			auto scalerList = reinterpret_cast<Il2CppArraySize * (*)(Il2CppObject*)>(il2cpp_class_get_method_from_name(uiManager->klass, "GetCanvasScalerList", 0)->methodPointer)(uiManager);
		//			cout << "scalerList: " << scalerList->max_length << "\n";
		//			if (scalerList) {
		//				for (int i = 0; i < scalerList->max_length; i++) {
		//					auto item = scalerList->vector[i];
		//					canvas_scaler_setres_hook(reinterpret_cast<Il2CppObject*>(item), Vector2_t{ width,height });
		//					reinterpret_cast<void (*)(Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
		//						"UnityEngine.UI.dll", "UnityEngine.UI",
		//						"CanvasScaler", "Handle", 0
		//					))(reinterpret_cast<Il2CppObject*>(item));
		//					auto methodPtr = il2cpp_symbols::get_method_pointer(
		//						"umamusume.dll", "Gallop", "UIManager", "UpdateCanvasScaler", 1);
		//					if (methodPtr && item) reinterpret_cast<void (*)(void*)>(methodPtr)(item);
		//				}
		//			}
		//			else {
		//				cout << "scalerList == nullptr";
		//			}

		//			/*auto enumerator1 = reinterpret_cast<Il2CppObject * (*)()>(il2cpp_symbols::get_method_pointer(
		//				"umamusume.dll",
		//				"Gallop",
		//				"Screen", "ChangeScreenScaleForPC", -1))();
		//			auto move_next1 = reinterpret_cast<void* (*)(
		//				Il2CppObject * _this)>(il2cpp_class_get_method_from_name(enumerator1->klass,
		//					"MoveNext",
		//					0)->methodPointer);
		//			move_next1(enumerator1);*/
		//		}
		//	}
		//}
		if (uMsg == WM_SYSCOMMAND)
		{
			if (wParam == SC_MAXIMIZE)
			{
				return TRUE;
			}
		}
		if (uMsg == WM_SYSKEYDOWN)
		{
			bool altDown = (lParam & (static_cast<long long>(1) << 29)) != 0;
			if (g_unlock_size &&
				wParam == VK_RETURN &&
				altDown &&
				!altEnterPressed)
			{
				altEnterPressed = true;
				if (!is_virt())
				{
					if (!fullScreenFlOverride)
					{
						fullScreenFlOverride = true;
					}
					fullScreenFl = !fullScreenFl;
					Resolution_t r;
					get_resolution_stub(&r);
					if (!fullScreenFl)
					{
						r.width = last_hriz_window_height;
						r.height = last_hriz_window_width;
						if (r.width < r.height)
						{
							r.width = last_hriz_window_width;
							r.height = last_hriz_window_height;
						}
					}
					else
					{
						RECT windowRect;
						GetClientRect(hWnd, &windowRect);
						last_hriz_window_width = windowRect.right - windowRect.left;
						last_hriz_window_height = windowRect.bottom - windowRect.top;
					}
					reinterpret_cast<decltype(set_resolution_hook)*>(set_resolution_orig)(r.width, r.height, fullScreenFl);
				}
				return TRUE;

			}
			if (g_max_fps > -1 && wParam == 'F' && altDown)
			{
				useDefaultFPS = !useDefaultFPS;
				set_fps_hook(30);
				return TRUE;
			}
		}
		if (g_unlock_size)
		{

			if (uMsg == WM_SYSKEYUP)
			{
				if (wParam == VK_RETURN && (lParam & (static_cast<long long>(1) << 29)) != 0)
				{
					altEnterPressed = false;
					return TRUE;
				}
			}
			if (uMsg == WM_SIZING)
			{
				RECT* rect = reinterpret_cast<RECT*>(lParam);

				float ratio = is_virt() ? 1.f / g_aspect_ratio : g_aspect_ratio;

				RECT windowRect;
				GetWindowRect(hWnd, &windowRect);
				int windowWidth = windowRect.right - windowRect.left,
					windowHeight = windowRect.bottom - windowRect.top;

				RECT clientRect;
				GetClientRect(hWnd, &clientRect);
				int clientWidth = (clientRect.right - clientRect.left),
					clientHeight = (clientRect.bottom - clientRect.top);

				float width = rect->right - rect->left;
				float height = rect->bottom - rect->top;

				int borderWidth = windowWidth - clientWidth;
				int borderHeight = windowHeight - clientHeight;

				rect->right -= borderWidth;
				rect->bottom -= borderHeight;

				switch (wParam)
				{
				case WMSZ_LEFT:
				case WMSZ_LEFT + WMSZ_BOTTOM:
					rect->left = rect->right - width;
					rect->bottom = rect->top + roundf(width / ratio);
					break;
				case WMSZ_LEFT + WMSZ_TOP:
					rect->left = rect->right - width;
					rect->top = rect->bottom - roundf(width / ratio);
					break;
				case WMSZ_RIGHT:
				case WMSZ_RIGHT + WMSZ_BOTTOM:
					rect->right = rect->left + width;
					rect->bottom = rect->top + roundf(width / ratio);
					break;
				case WMSZ_RIGHT + WMSZ_TOP:
					rect->right = rect->left + width;
					rect->top = rect->bottom - roundf(width / ratio);
					break;
				case WMSZ_TOP:
					rect->top = rect->bottom - height;
					rect->right = rect->left + roundf(height * ratio);
					break;
				case WMSZ_BOTTOM:
					rect->bottom = rect->top + height;
					rect->right = rect->left + roundf(height * ratio);
					break;
				}

				if (width > height)
				{
					last_hriz_window_width = rect->right - rect->left;
					last_hriz_window_height = rect->bottom - rect->top;
				}
				else
				{
					last_virt_window_width = rect->right - rect->left;
					last_virt_window_height = rect->bottom - rect->top;
				}

				rect->right += borderWidth;
				rect->bottom += borderHeight;

				return TRUE;
			}
		}
		if (uMsg == WM_CLOSE)
		{
			exit(0);
			return TRUE;
		}
		return reinterpret_cast<decltype(wndproc_hook)*>(wndproc_orig)(hWnd, uMsg, wParam, lParam);
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

	void* on_populate_orig = nullptr;
	void on_populate_hook(Il2CppObject* _this, void* toFill)
	{
		if (g_replace_to_builtin_font && text_get_linespacing(_this) != 1.05f)
		{
			text_set_style(_this, 1);
			text_set_size(_this, text_get_size(_this) - 4);
			text_set_linespacing(_this, 1.05f);
		}
		if (g_replace_to_custom_font)
		{
			auto font = text_get_font(_this);
			Il2CppString* name = uobject_get_name(font);
			if (g_font_asset_name.find(local::wide_u8(name->start_char)) == string::npos)
			{
				text_set_font(_this, GetCustomFont());
			}
		}

		return reinterpret_cast<decltype(on_populate_hook)*>(on_populate_orig)(_this, toFill);
	}

	void* textcommon_awake_orig = nullptr;
	void textcommon_awake_hook(Il2CppObject* _this)
	{
		if (g_replace_to_builtin_font)
		{
			text_assign_font(_this);
		}
		if (g_replace_to_custom_font)
		{
			auto customFont = GetCustomFont();
			if (customFont)
			{
				text_set_font(_this, customFont);
			}
		}
		text_set_text(_this, local::get_localized_string(text_get_text(_this)));
		reinterpret_cast<decltype(textcommon_awake_hook)*>(textcommon_awake_orig)(_this);
	}

	void* load_zekken_composite_resource_orig = nullptr;
	void load_zekken_composite_resource_hook(Il2CppObject* _this)
	{
		if (assets && g_replace_to_custom_font)
		{
			auto font = GetCustomFont();
			if (font)
			{
				FieldInfo* zekkenFontField = il2cpp_class_get_field_from_name(_this->klass, "_fontZekken");
				il2cpp_field_set_value(_this, zekkenFontField, font);
			}
		}
		reinterpret_cast<decltype(load_zekken_composite_resource_hook)*>(load_zekken_composite_resource_orig)(_this);
	}

	void* wait_resize_ui_orig = nullptr;
	Il2CppObject* wait_resize_ui_hook(Il2CppObject* _this, bool isPortrait, bool isShowOrientationGuide)
	{
		return reinterpret_cast<decltype(wait_resize_ui_hook)*>(wait_resize_ui_orig)(_this, g_force_landscape ? false : isPortrait, g_ui_loading_show_orientation_guide ? false : isShowOrientationGuide);
	}

	void* get_modified_string_orig = nullptr;
	Il2CppString* get_modified_string_hook(Il2CppString* text, Il2CppObject* input, bool allowNewLine)
	{
		if (!allowNewLine)
		{
			auto u8str = local::wide_u8(text->start_char);
			replaceAll(u8str, "\n", "");
			return il2cpp_string_new(u8str.data());
		}
		return text;
	}

	void* set_anti_aliasing_orig = nullptr;
	void set_anti_aliasing_hook(int level)
	{
		reinterpret_cast<decltype(set_anti_aliasing_hook)*>(set_anti_aliasing_orig)(g_anti_aliasing);
	}

	void* apply_graphics_quality_orig = nullptr;
	void apply_graphics_quality_hook(Il2CppObject* _this, int quality, bool force)
	{
		reinterpret_cast<decltype(apply_graphics_quality_hook)*>(apply_graphics_quality_orig)(_this, g_graphics_quality, true);
	}

	void* load_one_orig = nullptr;
	Boolean load_one_hook(Il2CppObject* _this, Il2CppObject* handle, Il2CppObject* request)
	{
		FieldInfo* hNameField = il2cpp_class_get_field_from_name(request->klass, "hname");
		Il2CppString* hName = nullptr;
		il2cpp_field_get_value(request, hNameField, &hName);
		auto hNameStr = local::wide_u8(hName->start_char);

		if (g_replace_assets.find(hNameStr) != g_replace_assets.end())
		{
			auto& replaceAsset = g_replace_assets.at(hNameStr);
			if (!replaceAsset.path.empty()) {
				auto set_assetBundle = reinterpret_cast<void (*)(
					Il2CppObject * _this, Il2CppObject * assetBundle)>(il2cpp_symbols::get_method_pointer(
						"_Cyan.dll", "Cyan.Loader", "AssetHandle", "SetAssetBundle",
						1));

				auto get_IsLoaded = reinterpret_cast<Boolean(*)(
					Il2CppObject * _this)>(il2cpp_symbols::get_method_pointer(
						"_Cyan.dll", "Cyan.Loader", "AssetHandle", "get_IsLoaded",
						0));

				if (!replaceAsset.asset)
				{
					replaceAsset.asset = load_from_file(il2cpp_string_new(replaceAsset.path.data()));
				}
				set_assetBundle(handle, replaceAsset.asset);
				return get_IsLoaded(handle);
			}
		}
		return reinterpret_cast<decltype(load_one_hook)*>(load_one_orig)(_this, handle, request);
	}

	void* assetbundle_unload_orig = nullptr;
	void assetbundle_unload_hook(Il2CppObject* _this, Boolean unloadAllLoadedObjects)
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

	void* ChangeScreenOrientation_orig = nullptr;

	Il2CppObject* ChangeScreenOrientation_hook(ScreenOrientation targetOrientation, bool isForce) {
		return reinterpret_cast<decltype(ChangeScreenOrientation_hook)*>(ChangeScreenOrientation_orig)(
			g_force_landscape ? ScreenOrientation::Landscape : targetOrientation, g_force_landscape ? false : isForce);
	}

	void* ChangeScreenOrientationPortraitAsync_orig = nullptr;

	Il2CppObject* ChangeScreenOrientationPortraitAsync_hook() {
		return reinterpret_cast<Il2CppObject * (*)()>(il2cpp_symbols::get_method_pointer(
			"umamusume.dll",
			"Gallop",
			"Screen", "ChangeScreenOrientationLandscapeAsync", -1))();
	}

	void* get_IsVertical_orig = nullptr;

	Boolean get_IsVertical_hook() {
		return GetBoolean(false);
	}

	void* Screen_set_orientation_orig = nullptr;

	void Screen_set_orientation_hook(ScreenOrientation orientation)
	{
		if ((orientation == ScreenOrientation::Portrait ||
			orientation == ScreenOrientation::PortraitUpsideDown) && g_force_landscape) {
			orientation = ScreenOrientation::Landscape;
		}
		reinterpret_cast<decltype(Screen_set_orientation_hook)*>(Screen_set_orientation_orig)(
			orientation);
	}

	void* DeviceOrientationGuide_Show_orig = nullptr;

	void DeviceOrientationGuide_Show_hook(Il2CppObject* _this, bool isTargetOrientationPortrait,
		int target)
	{
		reinterpret_cast<decltype(DeviceOrientationGuide_Show_hook)*>(DeviceOrientationGuide_Show_orig)(
			_this,
			!g_force_landscape && isTargetOrientationPortrait, g_force_landscape ? 2 : target);
	}

	void* NowLoading_Show_orig = nullptr;

	void NowLoading_Show_hook(Il2CppObject* _this, int type, Il2CppObject* onComplete,
		float overrideDuration)
	{
		// NowLoadingOrientation
		if (type == 2 && (g_force_landscape || !g_ui_loading_show_orientation_guide))
		{
			// NowLoadingTips
			type = 0;
		}
		reinterpret_cast<decltype(NowLoading_Show_hook)*>(NowLoading_Show_orig)(
			_this,
			type,
			onComplete, overrideDuration);
	}

	void* WaitDeviceOrientation_orig = nullptr;

	void WaitDeviceOrientation_hook(ScreenOrientation targetOrientation)
	{
		if ((targetOrientation == ScreenOrientation::Portrait ||
			targetOrientation == ScreenOrientation::PortraitUpsideDown) &&
			g_force_landscape)
		{
			targetOrientation = ScreenOrientation::Landscape;
		}
		reinterpret_cast<decltype(WaitDeviceOrientation_hook)*>(WaitDeviceOrientation_orig)(
			targetOrientation);
	}

	void* BootSystem_Awake_orig = nullptr;

	void BootSystem_Awake_hook(Il2CppObject* _this)
	{
		Resolution_t r;
		get_resolution_stub(&r);
		last_display_width = r.width;
		last_display_height = r.height;
		reinterpret_cast<decltype(BootSystem_Awake_hook)*>(BootSystem_Awake_orig)(_this);
	}

	void adjust_size()
	{
		thread([]() {
			auto tr = il2cpp_thread_attach(il2cpp_domain_get());

			Resolution_t r;
			get_resolution_stub(&r);

			auto target_height = r.height - 100;

			set_resolution_hook(target_height * 0.5625f, target_height, false);

			il2cpp_thread_detach(tr);
			}).detach();
	}

	void* load_scene_internal_orig = nullptr;
	void* load_scene_internal_hook(Il2CppString* sceneName, int sceneBuildIndex, void* parameters, bool mustCompleteNextFrame)
	{
		wprintf(L"%s\n", sceneName->start_char);
		return reinterpret_cast<decltype(load_scene_internal_hook)*>(load_scene_internal_orig)(sceneName, sceneBuildIndex, parameters, mustCompleteNextFrame);
	}

	void dump_all_entries()
	{
		// 0 is None
		for (int i = 1;; i++)
		{
			auto* str = reinterpret_cast<decltype(localize_get_hook)*>(localize_get_orig)(i);

			if (str && *str->start_char)
			{
				logger::write_entry(i, str->start_char);
			}
			else
			{
				// check next string, if it's still empty, then we are done!
				auto* nextStr = reinterpret_cast<decltype(localize_get_hook)*>(localize_get_orig)(i + 1);
				if (!(nextStr && *nextStr->start_char))
					break;
			}
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

#pragma region HOOK_MACRO
#define ADD_HOOK(_name_, _fmt_) \
	auto _name_##_offset = reinterpret_cast<void*>(_name_##_addr); \
	\
	/*printf(_fmt_, _name_##_offset); */\
	/*dump_bytes(_name_##_offset); */\
	\
	MH_CreateHook(_name_##_offset, _name_##_hook, &_name_##_orig); \
	MH_EnableHook(_name_##_offset); 
#pragma endregion
#pragma region HOOK_ADDRESSES
		load_assets = reinterpret_cast<Il2CppObject * (*)(Il2CppObject * _this, Il2CppString * name, Il2CppObject * runtimeType)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.AssetBundleModule.dll", "UnityEngine",
				"AssetBundle", "LoadAsset", 2)
			);

		uobject_get_name = reinterpret_cast<Il2CppString * (*)(Il2CppObject * uObject)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll", "UnityEngine",
				"Object", "GetName", -1)
			);

		get_unityVersion = reinterpret_cast<Il2CppString * (*)()>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll", "UnityEngine",
				"Application", "get_unityVersion", -1)
			);

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

		auto text_get_text_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.UI.dll", "UnityEngine.UI", "Text", "get_text", 0
		);

		auto localizeextension_text_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "LocalizeExtention", "Text", 1
		);

		// have to do this way because there's Get(TextId id) and Get(string id)
		// the string one looks like will not be called by elsewhere
		auto localize_get_addr = il2cpp_symbols::find_method("umamusume.dll", "Gallop", "Localize", [](const MethodInfo* method) {
			return method->name == "Get"s &&
				method->parameters->parameter_type->type == IL2CPP_TYPE_VALUETYPE;
			});

		auto text_clip_data_ctor_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineTextClipData", "FinishTypewrite", 0
		);

		auto cutin_timeline_controller_GetCurrentSpeed_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "CutInHelper", "GetTargetSpeed", 0
		);

		auto frame_to_time_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "TweenAnimationTimelineUtil", "FrameToTime", 1
		);

		auto time_to_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "TweenAnimationTimelineUtil", "TimeToFrame", 1
		);

		auto update_addr = il2cpp_symbols::get_method_pointer(
			"DOTween.dll", "DG.Tweening.Core", "TweenManager", "Update", 3
		);

		auto timeline_audioclip_ctor_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineData", "UpdateTimelineData", 3
		);

		/*auto get_start_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineBlockData", "get_StartFrame", 0
		);*/

		/*auto get_end_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineBlockData", "get_EndFrame", 0
		);*/

		/*auto get_end_frame1_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineClipData", "get_EndFrame", 0
		);*/

		auto update_timeline_data_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineBlockData", "UpdateBlockData", 4
		);

		/*auto get_fixed_start_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineClipData", "get_FixedStartFrame", 0
		);*/

		/*auto get_fixed_end_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StoryTimelineClipData", "get_FixedEndFrame", 0
		);*/

		auto get_time_from_frame_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop.CutIn.Cutt", "CutInTimelineController", "GetTimeFromFrame", 2
		);

		auto query_ctor_addr = il2cpp_symbols::get_method_pointer(
			"LibNative.Runtime.dll", "LibNative.Sqlite3",
			"Query", ".ctor", 2
		);

		auto query_getstr_addr = il2cpp_symbols::get_method_pointer(
			"LibNative.Runtime.dll", "LibNative.Sqlite3",
			"Query", "GetText", 1
		);

		auto query_dispose_addr = il2cpp_symbols::get_method_pointer(
			"LibNative.Runtime.dll", "LibNative.Sqlite3",
			"Query", "Dispose", 0
		);

		auto set_fps_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Application", "set_targetFrameRate", 1
		);

		auto wndproc_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"StandaloneWindowResize", "WndProc", 4
		);

		auto get_virt_size_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"StandaloneWindowResize", "getOptimizedWindowSizeVirt", 2
		);

		auto get_hori_size_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"StandaloneWindowResize", "getOptimizedWindowSizeHori", 2
		);

		display_get_main = reinterpret_cast<Il2CppObject * (*)()>(il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_main", -1));

		get_system_width = reinterpret_cast<int (*)(Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_systemWidth", 0));

		get_system_height = reinterpret_cast<int (*)(
			Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll",
				"UnityEngine",
				"Display", "get_systemHeight", 0));

		get_rendering_width = reinterpret_cast<int (*)(Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll",
			"UnityEngine",
			"Display", "get_renderingWidth", 0));

		get_rendering_height = reinterpret_cast<int (*)(
			Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll",
				"UnityEngine",
				"Display", "get_renderingHeight", 0));

		is_virt = reinterpret_cast<bool(*)()>(
			il2cpp_symbols::get_method_pointer(
				"umamusume.dll", "Gallop",
				"StandaloneWindowResize", "get_IsVirt", 0
			));

		get_resolution = reinterpret_cast<Resolution_t * (*)(Resolution_t*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll", "UnityEngine",
				"Screen", "get_currentResolution", 0
			)
			);

		auto story_timeline_controller_play_addr = il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "StoryTimelineController", "Play", 0);

		auto story_race_textasset_load_addr = il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "StoryRaceTextAsset", "Load", 0);

		auto get_modified_string_addr = il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "GallopUtil", "GetModifiedString", -1);

		auto gallop_get_screenheight_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"Screen", "get_Height", 0
		);

		auto gallop_get_screenwidth_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"Screen", "get_Width", 0
		);

		auto change_resize_ui_for_pc_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"UIManager", "ChangeResizeUIForPC", 2
		);

		auto canvas_scaler_setres_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.UI.dll", "UnityEngine.UI",
			"CanvasScaler", "set_referenceResolution", 1
		);

		set_scale_factor = reinterpret_cast<void(*)(void*, float)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"CanvasScaler", "set_scaleFactor", 1
			)
			);

		auto on_populate_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"TextCommon", "OnPopulateMesh", 1
		);

		auto textcommon_awake_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop",
			"TextCommon", "Awake", 0
		);

		text_get_text = reinterpret_cast<Il2CppString * (*)(void*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "get_text", 0
			)
			);
		text_set_text = reinterpret_cast<void (*)(void*, Il2CppString*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "set_text", 1
			)
			);

		text_assign_font = reinterpret_cast<void(*)(void*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "AssignDefaultFont", 0
			)
			);

		text_set_font = reinterpret_cast<void (*)(void*, Il2CppObject*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "set_font", 1
			)
			);

		text_get_font = reinterpret_cast<Il2CppObject * (*)(void*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "get_font", 0
			)
			);

		text_get_size = reinterpret_cast<int(*)(void*)>(
			il2cpp_symbols::get_method_pointer(
				"umamusume.dll", "Gallop",
				"TextCommon", "get_FontSize", 0
			)
			);

		text_set_size = reinterpret_cast<void(*)(void*, int)>(
			il2cpp_symbols::get_method_pointer(
				"umamusume.dll", "Gallop",
				"TextCommon", "set_FontSize", 1
			)
			);

		text_get_linespacing = reinterpret_cast<float(*)(void*)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "get_lineSpacing", 0
			)
			);

		text_set_style = reinterpret_cast<void(*)(void*, int)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "set_fontStyle", 1
			)
			);

		text_set_linespacing = reinterpret_cast<void(*)(void*, float)>(
			il2cpp_symbols::get_method_pointer(
				"UnityEngine.UI.dll", "UnityEngine.UI",
				"Text", "set_lineSpacing", 1
			)
			);

		auto set_resolution_addr = il2cpp_symbols::get_method_pointer(
			"UnityEngine.CoreModule.dll", "UnityEngine",
			"Screen", "SetResolution", 3
		);

		auto an_text_fix_data_addr = reinterpret_cast<void (*)(Il2CppObject * _this)>(il2cpp_symbols::get_method_pointer("Plugins.dll", "AnimateToUnity", "AnText", "_FixData", 0));

		auto an_text_set_material_to_textmesh_addr = reinterpret_cast<void (*)(Il2CppObject * _this)>(il2cpp_symbols::get_method_pointer("Plugins.dll", "AnimateToUnity", "AnText", "_SetMaterialToTextMesh", 0));

		auto load_zekken_composite_resource_addr = reinterpret_cast<void (*)(Il2CppObject * _this)>(il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "ModelLoader", "LoadZekkenCompositeResourceInternal", 0));

		auto wait_resize_ui_addr = reinterpret_cast<void (*)(Il2CppObject * _this, bool isPortrait, bool isShowOrientationGuide)>(il2cpp_symbols::get_method_pointer("umamusume.dll", "Gallop", "UIManager", "WaitResizeUI", 2));

		auto set_anti_aliasing_addr = reinterpret_cast<void (*)(
			int)>(il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
				"QualitySettings", "set_antiAliasing", 1));

		auto apply_graphics_quality_addr = reinterpret_cast<void (*)(
			Il2CppObject*, Il2CppObject*, bool)>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop",
				"GraphicSettings", "ApplyGraphicsQuality", 2));

		auto ChangeScreenOrientation_addr = reinterpret_cast<void (*)(
			ScreenOrientation, bool)>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop",
				"Screen", "ChangeScreenOrientation", 2));

		auto Screen_set_orientation_addr = reinterpret_cast<void (*)(
			ScreenOrientation)>(il2cpp_symbols::get_method_pointer(
				"UnityEngine.CoreModule.dll",
				"UnityEngine",
				"Screen", "set_orientation", 1));

		auto DeviceOrientationGuide_Show_addr = reinterpret_cast<void (*)(bool,
			int)>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop", "DeviceOrientationGuide", "Show", 2));

		auto NowLoading_Show_addr = reinterpret_cast<void (*)(int, Il2CppObject*,
			float)>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop", "NowLoading", "Show", 3));

		auto WaitDeviceOrientation_addr = reinterpret_cast<void (*)(
			ScreenOrientation)>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop", "Screen", "WaitDeviceOrientation", 1));

		auto BootSystem_Awake_addr = reinterpret_cast<void (*)(Il2CppObject*)>(il2cpp_symbols::get_method_pointer(
			"umamusume.dll",
			"Gallop", "BootSystem", "Awake", 0));

		auto UIManager_ChangeResizeUIForPC_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "UIManager", "ChangeResizeUIForPC", 2);

		auto GetLimitSize_addr = il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "StandaloneWindowResize", "GetLimitSize", -1);

		auto ChangeScreenOrientationPortraitAsync_addr = reinterpret_cast<Il2CppObject * (*)()>(il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "Screen", "ChangeScreenOrientationPortraitAsync", -1));

		auto get_IsVertical_addr = reinterpret_cast<Boolean(*)()>(il2cpp_symbols::get_method_pointer(
			"umamusume.dll", "Gallop", "Screen", "get_IsVertical", -1));

		load_from_file = reinterpret_cast<Il2CppObject * (*)(Il2CppString * path)>(il2cpp_symbols::get_method_pointer(
			"UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle",
			"LoadFromFile", 1));

		auto assetbundle_unload_addr = reinterpret_cast<Il2CppObject * (*)(Il2CppString * path)>(il2cpp_symbols::get_method_pointer("UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "Unload", 1));

		auto load_one_addr = reinterpret_cast<Boolean(*)(Il2CppObject * _this, Il2CppObject * handle, Il2CppObject * request)>(il2cpp_symbols::get_method_pointer("_Cyan.dll", "Cyan.Loader", "AssetLoader", "LoadOne", 2));

		auto load_scene_internal_addr = il2cpp_resolve_icall("UnityEngine.SceneManagement.SceneManager::LoadSceneAsyncNameIndexInternal_Injected(System.String,System.Int32,UnityEngine.SceneManagement.LoadSceneParameters&,System.bool)");
#pragma endregion
#pragma region LOAD_ASSETBUNDLE
		if (!assets && !g_font_assetbundle_path.empty() && g_replace_to_custom_font)
		{
			auto assetbundlePath = local::u8_wide(g_font_assetbundle_path);
			if (PathIsRelativeW(assetbundlePath.data()))
			{
				assetbundlePath.insert(0, ((wstring)std::filesystem::current_path().native()).append(L"/"));
			}
			assets = load_from_file(il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()));

			if (!assets && filesystem::exists(assetbundlePath))
			{
				cout << "Asset founded but not loaded. Maybe Asset BuildTarget is not for Windows\n";
			}
		}

		if (assets) {
			cout << "Asset loaded: " << assets << "\n";
		}
#pragma endregion

		ADD_HOOK(GetLimitSize, "Gallop.StandaloneWindowResize::GetChangedSize at %p\n");

		ADD_HOOK(UIManager_ChangeResizeUIForPC, "Gallop.UIManager::ChangeResizeUIForPC at %p\n");

		ADD_HOOK(NowLoading_Show, "Gallop.NowLoading::Show at %p\n");

		ADD_HOOK(assetbundle_unload, "UnityEngine.AssetBundle::Unload at %p\n");

		ADD_HOOK(load_one, "Cyan.Loader.AssetLoader::LoadOne at %p\n");

		ADD_HOOK(get_preferred_width, "UnityEngine.TextGenerator::GetPreferredWidth at %p\n");

		ADD_HOOK(an_text_fix_data, "AnimateToUnity.AnText::_FixData at %p\n");

		ADD_HOOK(an_text_set_material_to_textmesh, "AnimateToUnity.AnText::_SetMaterialToTextMesh at %p\n");

		ADD_HOOK(load_zekken_composite_resource, "Gallop.ModelLoader::LoadZekkenCompositeResource at %p\n");

		// ADD_HOOK(wait_resize_ui, "Gallop.UIManager::WaitResizeUI at %p\n");

		// hook UnityEngine.TextGenerator::PopulateWithErrors to modify text
		ADD_HOOK(populate_with_errors, "UnityEngine.TextGenerator::PopulateWithErrors at %p\n");

		// ADD_HOOK(text_get_text, "UnityEngine.UI.Text::get_text at %p\n");

		ADD_HOOK(localizeextension_text, "Gallop.LocalizeExtention.Text(TextId) at %p\n");
		// Looks like they store all localized texts that used by code in a dict
		ADD_HOOK(localize_get, "Gallop.Localize.Get(TextId) at %p\n");

		// ADD_HOOK(text_clip_data_ctor, "Gallop.StoryTimelineTextClipData::ctor at %p\n")

		ADD_HOOK(story_timeline_controller_play, "StoryTimelineController::Play at %p\n");

		ADD_HOOK(story_race_textasset_load, "StoryRaceTextAsset.Load at %p\n");

		ADD_HOOK(get_modified_string, "GallopUtil_GetModifiedString at %p\n");

		ADD_HOOK(update, "DG.Tweening.Core.TweenManager::Update at %p\n");

		// ADD_HOOK(complete, "DG.Tweening.Core.TweenManager::Complete at %p\n");

		// ADD_HOOK(get_start_frame, "Gallop.StoryTimelineBlockData::get_StartFrame at %p\n");

		// ADD_HOOK(get_end_frame, "Gallop.StoryTimelineBlockData::get_EndFrame at %p\n");

		// ADD_HOOK(get_end_frame1, "Gallop.StoryTimelineClipData::get_EndFrame at %p\n");

		// ADD_HOOK(update_timeline_data, "Gallop.StoryTimelineBlockData::UpdateBlockData at %p\n");

		// ADD_HOOK(get_fixed_end_frame, "Gallop.StoryTimelineClipData::get_FixedEndFrame at %p\n");

		// ADD_HOOK(timeline_audioclip_ctor, "Gallop.StoryTimelineController::GetTimeScaleHighSpeed at %p\n");

		ADD_HOOK(query_ctor, "Query::ctor at %p\n");
		ADD_HOOK(query_getstr, "Query::GetString at %p\n");
		ADD_HOOK(query_dispose, "Query::Dispose at %p\n");

		// ADD_HOOK(load_scene_internal, "SceneManager::LoadSceneAsyncNameIndexInternal at %p\n");

		if (g_force_landscape || g_unlock_size)
		{
			ADD_HOOK(BootSystem_Awake, "Gallop.BootSystem::Awake at %p\n");
		}

		if (g_force_landscape) {
			Resolution_t r;
			get_resolution_stub(&r);
			float new_ratio = static_cast<float>(r.width) / r.height;

			last_hriz_window_width = r.width - 400;
			last_hriz_window_height = last_hriz_window_width / new_ratio;
			ADD_HOOK(WaitDeviceOrientation, "Gallop.Screen::WaitDeviceOrientation at %p");
			ADD_HOOK(Screen_set_orientation, "Gallop.Screen::set_orientation at %p\n");
			ADD_HOOK(DeviceOrientationGuide_Show, "DeviceOrientationGuide::Show at %p\n");
			ADD_HOOK(ChangeScreenOrientation, "ChangeScreenOrientation at %p\n");
			ADD_HOOK(ChangeScreenOrientationPortraitAsync, "ChangeScreenOrientationPortraitAsync at %p\n");
			ADD_HOOK(get_IsVertical, "get_IsVertical at %p\n");
			auto enumerator = reinterpret_cast<Il2CppObject * (*)()>(il2cpp_symbols::get_method_pointer(
				"umamusume.dll",
				"Gallop",
				"Screen", "ChangeScreenOrientationLandscapeAsync", -1))();
			auto move_next = reinterpret_cast<void* (*)(
				Il2CppObject * _this)>(il2cpp_class_get_method_from_name(enumerator->klass,
					"MoveNext",
					0)->methodPointer);
			move_next(enumerator);
		}

		if (g_replace_to_builtin_font || g_replace_to_custom_font)
		{
			ADD_HOOK(on_populate, "Gallop.TextCommon::OnPopulateMesh at %p\n");
			ADD_HOOK(textcommon_awake, "Gallop.TextCommon::Awake at %p\n");
		}

		if (g_max_fps > -1)
		{
			// break 30-40fps limit
			ADD_HOOK(set_fps, "UnityEngine.Application.set_targetFrameRate at %p \n");
		}

		if (g_unlock_size)
		{
			// break 1080p size limit
			ADD_HOOK(get_virt_size, "Gallop.StandaloneWindowResize.getOptimizedWindowSizeVirt at %p \n");
			ADD_HOOK(get_hori_size, "Gallop.StandaloneWindowResize.getOptimizedWindowSizeHori at %p \n");

			// remove fixed 1080p render resolution
			ADD_HOOK(gallop_get_screenheight, "Gallop.Screen::get_Height at %p\n");
			ADD_HOOK(gallop_get_screenwidth, "Gallop.Screen::get_Width at %p\n");

			Resolution_t r;
			get_resolution_stub(&r);
			last_display_width = r.width;
			last_display_height = r.height;
		}

		if (g_max_fps > -1 || g_unlock_size)
		{
			ADD_HOOK(wndproc, "Gallop.StandaloneWindowResize.WndProc at %p \n");
		}

		if (g_unlock_size || g_force_landscape)
		{
			ADD_HOOK(canvas_scaler_setres, "UnityEngine.UI.CanvasScaler::set_referenceResolution at %p\n");
		}

		if (g_auto_fullscreen || g_force_landscape || g_unlock_size)
		{
			ADD_HOOK(set_resolution, "UnityEngine.Screen.SetResolution(int, int, bool) at %p\n");
			if (g_auto_fullscreen || g_force_landscape)
			{
				adjust_size();
			}
		}

		if (g_dump_entries)
			dump_all_entries();

		if (g_graphics_quality != -1) {
			ADD_HOOK(apply_graphics_quality, "Gallop.GraphicSettings.ApplyGraphicsQuality at %p\n");
		}

		if (g_anti_aliasing != -1) {
			ADD_HOOK(set_anti_aliasing, "UnityEngine.QualitySettings.set_antiAliasing(int) at %p\n");
		}
	}
}

bool init_hook()
{
	if (mh_inited)
		return false;

	if (MH_Initialize() != MH_OK)
		return false;

	mh_inited = true;

	MH_CreateHook(LoadLibraryW, load_library_w_hook, &load_library_w_orig);
	MH_EnableHook(LoadLibraryW);

	return true;
}

void uninit_hook()
{
	if (!mh_inited)
		return;

	MH_DisableHook(MH_ALL_HOOKS);
	MH_Uninitialize();
}
