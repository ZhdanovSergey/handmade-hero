#include "win32_handmade.hpp"

static bool global_is_app_running = true;
static bool global_is_pause = false;
static Screen global_screen = {}; // глобальный из-за main_window_callback

// TODO: WinMain должен быть максимально верхнеуровневым, вынести детали в отдельные процедуры (инициализации в конструктор)
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	WNDCLASSA window_class = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = main_window_callback,
		.hInstance = hInstance,
		.lpszClassName = "Handmade_Hero_Window_Class",
	};
	RegisterClassA(&window_class);

	HWND window = CreateWindowExA(0, window_class.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, nullptr, nullptr, hInstance, nullptr);
	HDC device_context = GetDC(window);
	
	global_screen.resize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

	Sound sound = {};
	sound.play(window);
	Game::Sound_Sample* game_sound_memory = (Game::Sound_Sample*)VirtualAlloc(nullptr, sound.get_buffer_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Debug_Marker debug_markers_array[TARGET_UPDATE_FREQUENCY - 1] = {};
	uptr debug_markers_index = 0;

	void* game_memory_base_address = nullptr;
	if constexpr (DEV_MODE && UINTPTR_MAX == UINT64_MAX) game_memory_base_address = (void*)1024_GB;
	Game::Memory game_memory = {
		.permanent_storage_size = 64_MB,
		.transient_storage_size = 1_GB,
		.permanent_storage = (u8*)VirtualAlloc(game_memory_base_address,
			game_memory.permanent_storage_size + game_memory.transient_storage_size,
			MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE),
		.transient_storage = game_memory.permanent_storage + game_memory.permanent_storage_size,
    	.read_file_sync = Platform::read_file_sync,
    	.write_file_sync = Platform::write_file_sync,
    	.free_file_memory = Platform::free_file_memory,
	};

	init_xinput();
	Game::Input game_input = {};

	Game_Code game_code = {};
	game_code.load();

	u64 flip_wall_clock = get_wall_clock();
	u64 flip_cycle_counter = __rdtsc();

	while (global_is_app_running) {
		game_input.reset_transitions_count();
		process_pending_messages(game_input.controllers[0]);
		process_gamepad_input(game_input.controllers[1]);

		if constexpr (DEV_MODE) {
			game_code.reload_if_recompiled();

			if (global_is_pause) {
				wait_until_end_of_frame(flip_wall_clock);
				flip_wall_clock = get_wall_clock();
				flip_cycle_counter = __rdtsc();
				continue;
			};
		}

		Game::Screen_Buffer game_screen_buffer = {
			.width  = global_screen.get_width(),
			.height = global_screen.get_height(),
			.memory = global_screen.memory,
		};

		game_code.update_and_render(game_input, game_memory, game_screen_buffer);
		sound.calc_required_output(flip_wall_clock, debug_markers_array, debug_markers_index);

		Game::Sound_Buffer game_sound_buffer = {
			.samples_per_second = sound.wave_format.nSamplesPerSec,
			.samples_to_write = sound.output_byte_count / sound.wave_format.nBlockAlign,
			.samples = game_sound_memory,
		};

		game_code.get_sound_samples(game_memory, game_sound_buffer);
		sound.fill(game_sound_buffer);

		wait_until_end_of_frame(flip_wall_clock);
		flip_wall_clock = get_wall_clock();
		flip_cycle_counter = __rdtsc();

		if constexpr (DEV_MODE) {
			bool is_cursors_recorded = sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(
				&debug_markers_array[debug_markers_index].flip_play_cursor, nullptr
			));
			if (is_cursors_recorded) {
				debug_sound_sync_display(
					global_screen, sound,
					debug_markers_array, utils::array_count(debug_markers_array), debug_markers_index
				);
			}

			// uptr debug_markers_index_unwrapped = debug_markers_index == 0 ? utils::array_count(debug_markers_array) : debug_markers_index;
			// Debug_Marker prev_marker = debug_markers_array[debug_markers_index_unwrapped - 1];
			// DWORD prev_sound_filled_cursor = (prev_marker.output_location + prev_marker.output_byte_count) % sound.get_buffer_size();
			// // сравниваем с половиной размера буфера чтобы учесть что prev_sound_filled_cursor мог сделать оборот
			// assert(sound.play_cursor <= prev_sound_filled_cursor || sound.play_cursor > prev_sound_filled_cursor + sound.get_buffer_size() / 2);
			
			// f32 ms_per_frame = 1000 * frame_seconds_elapsed;
			// u64 cycle_counter_elapsed = __rdtsc() - flip_cycle_counter;
			// char output_buffer[256];
			// sprintf_s(output_buffer, "frame ms: %.2f\n", frame_seconds_elapsed * 1000);
			// OutputDebugStringA(output_buffer);

			debug_markers_index = (debug_markers_index + 1) % utils::array_count(debug_markers_array);
		}

		global_screen.display(window, device_context);
	}

	return EXIT_SUCCESS;
}

static LRESULT CALLBACK main_window_callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			global_screen.display(window, device_context);
			EndPaint(window, &paint);
		} break;
		case WM_CLOSE:
		case WM_DESTROY: {
			global_is_app_running = false;
		} break;
		case WM_KEYUP:
		case WM_KEYDOWN: {
			assert(!"Keyboard input came in through non-dispatched message!");
		} break;
		default:
			return DefWindowProcA(window, message, wParam, lParam);
	}

	return 0;
}

Game_Code::Game_Code() {
	auto& game_code = *this;
	char main_file_path[MAX_PATH]; // бывают более длинные пути!
	GetModuleFileNameA(nullptr, main_file_path, sizeof(main_file_path));
	char* one_past_last_slash = main_file_path;
	for (char* scan = main_file_path; *scan; scan++) {
		if (*scan == '\\') one_past_last_slash = scan + 1;
	}
	uptr main_folder_path_size = (uptr)(one_past_last_slash - main_file_path);
	
	const char dll_name[] = "handmade.dll";
	utils::concat_strings(
		main_file_path, main_folder_path_size,
		dll_name, sizeof(dll_name),
		game_code.dll_path, sizeof(game_code.dll_path)
	);

	const char temp_dll_name[] = "handmade_temp.dll";
	utils::concat_strings(
		main_file_path, main_folder_path_size,
		temp_dll_name, sizeof(temp_dll_name),
		game_code.temp_dll_path, sizeof(game_code.temp_dll_path)
	);
}

void Game_Code::load() {
	auto& game_code = *this;
	BOOL copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);

	if constexpr (DEV_MODE) {
		while (!copy_result && GetLastError() == ERROR_SHARING_VIOLATION) {
			Sleep(1); // ждем в цикле когда Visual Studio освободит новый dll
			copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);
		}
	}

	HMODULE loaded_dll = LoadLibraryA(game_code.temp_dll_path); // загружаем копию, чтобы компилятор мог писать в оригинальный файл
	if (!loaded_dll) {
		game_code.update_and_render = Game::update_and_render_stub;
		game_code.get_sound_samples = Game::get_sound_samples_stub;
	} else {
		game_code.dll = loaded_dll;
		game_code.write_time = get_file_write_time(game_code.dll_path);
		game_code.update_and_render = (Game::Update_And_Render*)GetProcAddress(loaded_dll, "update_and_render");
		game_code.get_sound_samples = (Game::Get_Sound_Samples*)GetProcAddress(loaded_dll, "get_sound_samples");
	}
}

void Game_Code::reload_if_recompiled() {
	auto& game_code = *this;
	FILETIME dll_write_time = get_file_write_time(game_code.dll_path);
	bool is_dll_recompiled = CompareFileTime(&dll_write_time, &game_code.write_time) > 0;
	if (is_dll_recompiled) {
		FreeLibrary(game_code.dll);
		game_code.dll = nullptr;
		game_code.load();
	}
}

static FILETIME get_file_write_time(const char* filename) {
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &file_data)) return {};

	return file_data.ftLastWriteTime;
}

void Screen::display(HWND window, HDC device_context) const {
	auto& screen = *this;
	RECT client_rect;
	GetClientRect(window, &client_rect);

	int dest_width = client_rect.right - client_rect.left;
	int dest_height = client_rect.bottom - client_rect.top;
	int src_width = (int)screen.get_width();
	int src_height = (int)screen.get_height();

	StretchDIBits(device_context,
		0, 0, dest_width, dest_height,
		0, 0, src_width, src_height,
		screen.memory, &screen.bitmap_info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

void Screen::resize(u32 width, u32 height) {
	auto& screen = *this;
	screen.set_width(width);
	screen.set_height(height);
	uptr screen_memory_size = width * height * sizeof(*screen.memory);
	if (screen.memory) VirtualFree(screen.memory, 0, MEM_RELEASE);
	screen.memory = (u32*)VirtualAlloc(nullptr, screen_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void Sound::play(HWND window) {
	auto& sound = *this;
	HMODULE direct_sound_dll = LoadLibraryA("dsound.dll");
	if (!direct_sound_dll) return;

	typedef HRESULT WINAPI Direct_Sound_Create(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
	Direct_Sound_Create* DirectSoundCreate = (Direct_Sound_Create*)GetProcAddress(direct_sound_dll, "DirectSoundCreate");
	IDirectSound* direct_sound;
	if (!SUCCEEDED(DirectSoundCreate(nullptr, &direct_sound, nullptr))) return;
	if (!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

	DSBUFFERDESC primary_buffer_desc = {
		.dwSize = sizeof(primary_buffer_desc),
		.dwFlags = DSBCAPS_PRIMARYBUFFER
	};
	IDirectSoundBuffer* primary_buffer;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, nullptr))) return;
	if (!SUCCEEDED(primary_buffer->SetFormat(&sound.wave_format))) return;

	DSBUFFERDESC sound_buffer_desc = {
		.dwSize = sizeof(sound_buffer_desc),
		.dwBufferBytes = sound.get_buffer_size(),
		.lpwfxFormat = &sound.wave_format
	};
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr))) return;

	void *region1, *region2; DWORD region1_size, region2_size;
	if(!SUCCEEDED(sound.buffer->Lock(0, sound.get_buffer_size(), &region1, &region1_size, &region2, &region2_size, 0))) return;
	utils::memset(region1, 0, region1_size);
	utils::memset(region2, 0, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);

	// TODO: address sanitizer падает после вызова Play(), по-видимому это известная проблема DirectSound
	// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
}

void Sound::calc_required_output(u64 flip_wall_clock, Debug_Marker* debug_markers_array, uptr debug_markers_index) {
	// Определяем величину, на размер которой может отличаться время цикла (safety_bytes). Когда мы просыпаемся чтобы писать звук,
	// смотрим где находится play_cursor и делаем прогноз где от будет находиться при смене кадра (expected_flip_play_cursor_unwrapped).
	// Если write_cursor + safety_bytes < expected_flip_play_cursor_unwrapped, то это значит что у нас звуковая карта с маленькой задержкой
	// и мы успеваем писать звук синхронно с изображением, поэтому пишем звук до конца следующего фрейма.
	// Если write_cursor + safety_bytes >= expected_flip_play_cursor_unwrapped, то полностью синхронизировать звук и изображение не получится,
	// просто пишем количество сэмплов, равное bytes_per_frame + safety_bytes

	auto& sound = *this;
	f32 from_flip_to_audio_seconds = get_seconds_elapsed(flip_wall_clock);
	if (!sound.buffer || !SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.play_cursor, &sound.write_cursor))) {
		sound.running_sample_index = sound.write_cursor / sound.wave_format.nBlockAlign;
	}

	DWORD expected_bytes_until_flip = sound.bytes_per_frame - (DWORD)((f32)sound.wave_format.nAvgBytesPerSec * from_flip_to_audio_seconds);
	DWORD expected_flip_play_cursor_unwrapped = sound.play_cursor + expected_bytes_until_flip;
	DWORD write_cursor_unwrapped = sound.play_cursor < sound.write_cursor
		? sound.write_cursor
		: sound.write_cursor + sound.get_buffer_size();

	bool is_low_latency_sound = (write_cursor_unwrapped + sound.safety_bytes) < expected_flip_play_cursor_unwrapped;
	DWORD target_cursor_unwrapped = is_low_latency_sound
		? expected_flip_play_cursor_unwrapped + sound.bytes_per_frame
		: write_cursor_unwrapped + sound.bytes_per_frame + sound.safety_bytes;

	DWORD target_cursor = target_cursor_unwrapped % sound.get_buffer_size();
	sound.output_location = sound.running_sample_index * sound.wave_format.nBlockAlign % sound.get_buffer_size();
	sound.output_byte_count = sound.output_location < target_cursor ? 0 : sound.get_buffer_size();
	sound.output_byte_count += target_cursor - sound.output_location;

	if constexpr (DEV_MODE) {
		debug_markers_array[debug_markers_index].output_play_cursor = sound.play_cursor;
		debug_markers_array[debug_markers_index].output_write_cursor = sound.write_cursor;
		debug_markers_array[debug_markers_index].output_location = sound.output_location;
		debug_markers_array[debug_markers_index].output_byte_count = sound.output_byte_count;
		debug_markers_array[debug_markers_index].expected_flip_play_cursor = expected_flip_play_cursor_unwrapped % sound.get_buffer_size();
	}
}

void Sound::fill(const Game::Sound_Buffer& source) {
	auto& sound = *this;
	void *region1, *region2; DWORD region1_size, region2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.output_location, sound.output_byte_count, &region1, &region1_size, &region2, &region2_size, 0))) return;

	u32 sample_size = sizeof(*(source.samples));
	u32 region1_size_samples = region1_size / sample_size;
	u32 region2_size_samples = region2_size / sample_size;
	sound.running_sample_index += region1_size_samples + region2_size_samples;

	utils::memcpy(region1, source.samples, region1_size);
	utils::memcpy(region2, source.samples + region1_size_samples, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
}

// TODO: заинлайнить эту функцию в wWinMain чтобы разделить обработку инпута и глобальных переменных
static void process_pending_messages(Game::Controller& controller) {
	MSG message;
	while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
		switch (message.message) {
			case WM_QUIT: {
				global_is_app_running = false;
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN: {
				bool is_key_pressed  = !(message.lParam & (1u << 31));
				bool was_key_pressed =   message.lParam & (1u << 30);
				if (is_key_pressed == was_key_pressed) continue;

				if constexpr (DEV_MODE) {
					if (message.wParam == 'P' && message.message == WM_KEYDOWN) {
						global_is_pause = !global_is_pause;
					}
				}

				Game::Button_State* button_state;
				switch (message.wParam) {
					case VK_RETURN: button_state = &controller.start;			break;
					case VK_ESCAPE: button_state = &controller.back;			break;
					case VK_UP: 	button_state = &controller.move_up;			break;
					case VK_DOWN: 	button_state = &controller.move_down;		break;
					case VK_LEFT: 	button_state = &controller.move_left;		break;
					case VK_RIGHT: 	button_state = &controller.move_right;		break;
					case 'W': 		button_state = &controller.action_up;		break;
					case 'S': 		button_state = &controller.action_down;		break;
					case 'A': 		button_state = &controller.action_left;		break;
					case 'D': 		button_state = &controller.action_right;	break;
					case 'Q': 		button_state = &controller.left_shoulder;	break;
					case 'E': 		button_state = &controller.right_shoulder;	break;
					default: continue;
				}
				button_state->is_pressed = is_key_pressed;
				button_state->transitions_count++;
			} break;
			default: {
				TranslateMessage(&message);
				DispatchMessageA(&message);
			}
		}
	}
}

static void init_xinput() {
	HMODULE xinput_dll = LoadLibraryA("xinput1_3.dll");
	if (!xinput_dll) return;
	
	XInputGetState = (Xinput_Get_State*)GetProcAddress(xinput_dll, "XInputGetState");
	XInputSetState = (Xinput_Set_State*)GetProcAddress(xinput_dll, "XInputSetState");
}

static void process_gamepad_input(Game::Controller& controller) {
	XINPUT_STATE state;
	XINPUT_GAMEPAD& gamepad = state.Gamepad;
	if (XInputGetState(0, &state)) return;
	
	controller.is_analog = true;
	controller.start_x = controller.end_x;
	controller.start_y = controller.end_y;
	controller.end_x = controller.min_x = controller.max_x = get_normalized_stick_value(gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	controller.end_y = controller.min_y = controller.max_y = get_normalized_stick_value(gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

	process_gamepad_button(controller.start,			gamepad.wButtons & XINPUT_GAMEPAD_START);
	process_gamepad_button(controller.back,				gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	process_gamepad_button(controller.left_shoulder, 	gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	process_gamepad_button(controller.right_shoulder,	gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	process_gamepad_button(controller.move_up, 			gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	process_gamepad_button(controller.move_down, 		gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	process_gamepad_button(controller.move_left, 		gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	process_gamepad_button(controller.move_right, 		gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	process_gamepad_button(controller.action_up, 		gamepad.wButtons & XINPUT_GAMEPAD_A);
	process_gamepad_button(controller.action_down, 		gamepad.wButtons & XINPUT_GAMEPAD_B);
	process_gamepad_button(controller.action_left, 		gamepad.wButtons & XINPUT_GAMEPAD_X);
	process_gamepad_button(controller.action_right, 	gamepad.wButtons & XINPUT_GAMEPAD_Y);
}

static inline f32 get_normalized_stick_value(SHORT value, SHORT deadzone) {
	return (-deadzone <= value && value < 0) || (0 < value && value <= deadzone) ? 0 : value / 32768.0f;
}

static inline void process_gamepad_button(Game::Button_State& state, bool is_pressed) {
	state.transitions_count += is_pressed != state.is_pressed;
	state.is_pressed = is_pressed;
}

static inline u64 get_wall_clock() {
	LARGE_INTEGER performance_counter_result;
	QueryPerformanceCounter(&performance_counter_result);
	return (u64)performance_counter_result.QuadPart;
}

static inline f32 get_seconds_elapsed(u64 start) {
	return (f32)(get_wall_clock() - start) / (f32)PERFORMANCE_FREQUENCY;
}

static void wait_until_end_of_frame(u64 flip_wall_clock) {
	f32 frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	if (SLEEP_GRANULARITY_MS) {
		f32 sleep_ms = 1000.0f * (TARGET_SECONDS_PER_FRAME - frame_seconds_elapsed) - (f32)SLEEP_GRANULARITY_MS;
		if (sleep_ms >= 1) Sleep((DWORD)sleep_ms);
	}
	frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	while (frame_seconds_elapsed < TARGET_SECONDS_PER_FRAME) {
		YieldProcessor();
		frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
	}
}

namespace Platform {
	Read_File_Result read_file_sync(const char* file_name) {
		Read_File_Result result = {};
		u32 memory_size = 0;
		void* memory = nullptr;

		HANDLE heap_handle = GetProcessHeap();
		if (!heap_handle) return result;

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) return result;

		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(file_handle, &file_size)) goto close_file_handle;

		assert(file_size.QuadPart <= UINT32_MAX);
		memory_size = (u32)file_size.QuadPart;
		memory = HeapAlloc(heap_handle, 0, memory_size);
		if (!memory) goto close_file_handle;

		DWORD bytes_read;
		if (!ReadFile(file_handle, memory, memory_size, &bytes_read, nullptr) || (bytes_read != memory_size)) {
			HeapFree(heap_handle, 0, memory);
			goto close_file_handle;
		}

		result.memory_size = memory_size;
		result.memory = memory;

		close_file_handle:
			CloseHandle(file_handle);
			return result;
	}

	bool write_file_sync(const char* file_name, const void* memory, u32 memory_size) {
		bool result = false;
		HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) return result;

		DWORD bytes_written;
		if (WriteFile(file_handle, memory, memory_size, &bytes_written, nullptr) && (bytes_written == memory_size)) {
			result = true;
		}
		CloseHandle(file_handle);
		return result;
	}
	
	void free_file_memory(void* memory) {
		if (!memory) return;

		HANDLE heap_handle = GetProcessHeap();
		if (!heap_handle) return;

		HeapFree(heap_handle, 0, memory);
		memory = nullptr;
	}
}

static void debug_sound_sync_display(
	Screen& screen, const Sound& sound,
	const Debug_Marker* markers, uptr markers_count, uptr current_marker_index) {

	f32 horizontal_scaling = (f32)screen.get_width() / (f32)sound.get_buffer_size();
	Debug_Marker current_marker = markers[current_marker_index];

	u32 expected_flip_play_cursor_x = (u32)((f32)current_marker.expected_flip_play_cursor * horizontal_scaling);
	debug_draw_vertical(screen, expected_flip_play_cursor_x, 0, screen.get_height(), 0xffffff00);

	for (uptr i = 0; i < markers_count; i++) {
		u32 top = 0;
		u32 bottom = screen.get_height() * 1/4;
		u32 historic_output_play_cursor_x = (u32)((f32)markers[i].output_play_cursor * horizontal_scaling);
		u32 historic_output_write_cursor_x = (u32)((f32)markers[i].output_write_cursor * horizontal_scaling);
		debug_draw_vertical(screen, historic_output_play_cursor_x, top, bottom, 0x00ffffff);
		debug_draw_vertical(screen, historic_output_write_cursor_x, top, bottom, 0x00ff0000);
	}
	{
		u32 top = screen.get_height() * 1/4;
		u32 bottom = screen.get_height() * 2/4;
		u32 output_play_cursor_x = (u32)((f32)current_marker.output_play_cursor * horizontal_scaling);
		u32 output_write_cursor_x = (u32)((f32)current_marker.output_write_cursor * horizontal_scaling);
		debug_draw_vertical(screen, output_play_cursor_x, top, bottom, 0x00ffffff);
		debug_draw_vertical(screen, output_write_cursor_x, top, bottom, 0x00ff0000);
	}
	{
		u32 top = screen.get_height() * 2/4;
		u32 bottom = screen.get_height() * 3/4;
		u32 output_location_x = (u32)((f32)current_marker.output_location * horizontal_scaling);
		u32 output_byte_count_x = (u32)((f32)current_marker.output_byte_count * horizontal_scaling);
		debug_draw_vertical(screen, output_location_x, top, bottom, 0x00ffffff);
		debug_draw_vertical(screen, (output_location_x + output_byte_count_x) % screen.get_width(), top, bottom, 0x00ff0000);
	}
	{
		u32 top = screen.get_height() * 3/4;
		u32 bottom = screen.get_height();
		u32 flip_play_cursor_x = (u32)((f32)current_marker.flip_play_cursor * horizontal_scaling);
		u32 safety_bytes_x = (u32)((f32)sound.safety_bytes * horizontal_scaling);
		debug_draw_vertical(screen, (flip_play_cursor_x - safety_bytes_x / 2) % screen.get_width(), top, bottom, 0x00ffffff);
		debug_draw_vertical(screen, (flip_play_cursor_x + safety_bytes_x / 2) % screen.get_width(), top, bottom, 0x00ffffff);
	}
}

static void debug_draw_vertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color) {
	u32* pixel = screen.memory + top * screen.get_width() + x;

	for (u32 y = top; y < bottom; y++) {
		*pixel = color;
		pixel += screen.get_width();
	}
}