#include "win32_handmade.hpp"

static const u32 INITIAL_WINDOW_WIDTH = 1280;
static const u32 INITIAL_WINDOW_HEIGHT = 720;
static const u32 TARGET_UPDATE_FREQUENCY = 30;
static const f32 TARGET_SECONDS_PER_FRAME = 1.0f / (f32)TARGET_UPDATE_FREQUENCY;
static const u64 PERFORMANCE_FREQUENCY = get_performance_frequency();

static bool global_is_app_running = true;
static bool global_is_pause = false;

// глобальный из-за main_window_callback
static Screen global_screen = {
	.bitmap_info = {
		.bmiHeader = {
			.biSize = sizeof(BITMAPINFOHEADER),
			.biPlanes = 1,
			.biBitCount = 32,
			.biCompression = BI_RGB,
		},
	}
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
	
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	UINT sleep_granularity_ms = 1;
	if (timeBeginPeriod(sleep_granularity_ms) != TIMERR_NOERROR) {
		sleep_granularity_ms = 0;
	}

	WNDCLASSA window_class = {
		.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = main_window_callback,
		.hInstance = hInstance,
		.lpszClassName = "Handmade_Hero_Window_Class",
	};
	RegisterClassA(&window_class);
	HWND window = CreateWindowExA(
		0, window_class.lpszClassName, "Handmade Hero", WS_TILEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT,
		nullptr, nullptr, hInstance, nullptr
	);
	HDC device_context = GetDC(window);
	resize_screen_buffer(global_screen, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);

	Sound sound = {
		.wave_format = {
			.wFormatTag = WAVE_FORMAT_PCM,
			.nChannels = 2,
			.nSamplesPerSec = 48000,
			.nAvgBytesPerSec = sizeof(Game::Sound_Sample) * sound.wave_format.nSamplesPerSec,
			.nBlockAlign = sizeof(Game::Sound_Sample),
			.wBitsPerSample = sizeof(Game::Sound_Sample) / sound.wave_format.nChannels * 8,
		},
		.bytes_per_frame = sound.wave_format.nAvgBytesPerSec / TARGET_UPDATE_FREQUENCY,
		.safety_bytes = sound.bytes_per_frame / 3,
	};
	init_direct_sound(window, sound);
	Game::Sound_Sample* game_sound_memory = (Game::Sound_Sample*)VirtualAlloc(nullptr, sound.get_buffer_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (sound.buffer) {
		clear_sound_buffer(sound);
		// TODO: address sanitizer падает после вызова Play(), по-видимому это известная проблема DirectSound
		// https://stackoverflow.com/questions/72511236/directsound-crashes-due-to-a-read-access-violation-when-calling-idirectsoundbuff
		sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
	}
	Debug::Marker debug_markers_array[TARGET_UPDATE_FREQUENCY - 1] = {};
	uptr debug_markers_index = 0;

	void* game_memory_base_address = nullptr;
	if constexpr (DEV_MODE && UINTPTR_MAX == UINT64_MAX) {
		game_memory_base_address = (void*)1024_GB;
	}
	uptr game_memory_size = 1_GB;
	std::byte* game_memory_storage = (std::byte*)VirtualAlloc(game_memory_base_address, game_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	Game::Memory game_memory = {
		.permanent_storage_size = 64_MB,
		.transient_storage_size = game_memory_size - game_memory.permanent_storage_size,
		.permanent_storage = game_memory_storage,
		.transient_storage = game_memory.permanent_storage + game_memory.permanent_storage_size,
    	.read_file_sync = Platform::read_file_sync,
    	.write_file_sync = Platform::write_file_sync,
    	.free_file_memory = Platform::free_file_memory,
	};

	init_xinput();
	Game::Input game_input = {};

	Game_Code game_code = load_game_code();
	u32 game_code_load_counter = 0;

	u64 flip_wall_clock = get_wall_clock();
	u64 flip_cycle_counter = __rdtsc();

	while (global_is_app_running) {
		game_input.reset_transitions_count();
		process_pending_messages(game_input.controllers[0]);
		process_gamepad_input(game_input.controllers[1]);

		if constexpr (DEV_MODE) {
			if (game_code_load_counter++ > TARGET_UPDATE_FREQUENCY * 5) {
				game_code_load_counter = 0;
				unload_game_code(game_code);
				game_code = load_game_code();
			}

			if (global_is_pause) {
				YieldProcessor();
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
		calc_required_sound_output(sound, flip_wall_clock, debug_markers_array, debug_markers_index);

		Game::Sound_Buffer game_sound_buffer = {
			.samples_per_second = sound.wave_format.nSamplesPerSec,
			.samples_to_write = sound.output_byte_count / sound.wave_format.nBlockAlign,
			.samples = game_sound_memory,
		};

		game_code.get_sound_samples(game_memory, game_sound_buffer);
		fill_sound_buffer(game_sound_buffer, sound);

		f32 frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
		if (sleep_granularity_ms) {
			f32 sleep_ms = 1000.0f * (TARGET_SECONDS_PER_FRAME - frame_seconds_elapsed) - (f32)sleep_granularity_ms;
			if (sleep_ms >= 1) Sleep((DWORD)sleep_ms);
		}
		frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
		while (frame_seconds_elapsed < TARGET_SECONDS_PER_FRAME) {
			YieldProcessor();
			frame_seconds_elapsed = get_seconds_elapsed(flip_wall_clock);
		}
		flip_wall_clock = get_wall_clock();
		flip_cycle_counter = __rdtsc();

		if constexpr (DEV_MODE) {
			bool is_cursors_recorded = sound.buffer && SUCCEEDED(sound.buffer->GetCurrentPosition(
				&debug_markers_array[debug_markers_index].flip_play_cursor, nullptr
			));
			if (is_cursors_recorded) {
				Debug::sound_sync_display(
					global_screen, sound,
					debug_markers_array, array_count(debug_markers_array), debug_markers_index
				);
			}

			// uptr debug_markers_index_unwrapped = debug_markers_index == 0 ? array_count(debug_markers_array) : debug_markers_index;
			// Debug::Marker prev_marker = debug_markers_array[debug_markers_index_unwrapped - 1];
			// DWORD prev_sound_filled_cursor = (prev_marker.output_location + prev_marker.output_byte_count) % sound.get_buffer_size();
			// // сравниваем с половиной размера буфера чтобы учесть что prev_sound_filled_cursor мог сделать оборот
			// assert(sound.play_cursor <= prev_sound_filled_cursor || sound.play_cursor > prev_sound_filled_cursor + sound.get_buffer_size() / 2);
			
			// f32 ms_per_frame = 1000 * frame_seconds_elapsed;
			// u64 cycle_counter_elapsed = __rdtsc() - flip_cycle_counter;
			char output_buffer[256];
			sprintf_s(output_buffer, "frame ms: %.2f\n", frame_seconds_elapsed * 1000);
			OutputDebugStringA(output_buffer);

			debug_markers_index = (debug_markers_index + 1) % array_count(debug_markers_array);
		}

		display_screen_buffer(window, device_context, global_screen);
	}

	return EXIT_SUCCESS;
}

static LRESULT CALLBACK main_window_callback(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			display_screen_buffer(window, device_context, global_screen);
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

static Game_Code load_game_code() {
	// TODO: регулярные потери кадров из-за синхронного копирования файла
	CopyFileA("win32_dll_main.dll", "win32_dll_main_temp.dll", FALSE);
	HMODULE game_dll = LoadLibraryA("win32_dll_main_temp.dll"); // загружаем win32_dll_main_temp.dll чтобы компилятор мог писать в win32_dll_main.dll
	if (!game_dll) return {
		.update_and_render = Game::update_and_render_stub,
		.get_sound_samples = Game::get_sound_samples_stub,
	};

	return {
		.update_and_render = (Game::Update_And_Render*)GetProcAddress(game_dll, "update_and_render"),
		.get_sound_samples = (Game::Get_Sound_Samples*)GetProcAddress(game_dll, "get_sound_samples"),
		.game_dll = game_dll,
	};
}

static void unload_game_code(Game_Code& game_code) {
	if (!game_code.game_dll) return;

	FreeLibrary(game_code.game_dll);
	game_code = {
		.update_and_render = Game::update_and_render_stub,
		.get_sound_samples = Game::get_sound_samples_stub,
	};
}

static void display_screen_buffer(HWND window, HDC device_context, const Screen& screen) {
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

static void resize_screen_buffer(Screen& screen, u32 width, u32 height) {
	if (screen.memory) {
		VirtualFree(screen.memory, 0, MEM_RELEASE);
	}
	
	screen.set_width(width);
	screen.set_height(height);
	uptr screen_memory_size = width * height * sizeof(*screen.memory);
	screen.memory = (Game::Screen_Pixel*)VirtualAlloc(nullptr, screen_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void init_direct_sound(HWND window, Sound& sound) {
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
	direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr);
}

static void calc_required_sound_output(Sound& sound, u64 flip_wall_clock, Debug::Marker* debug_markers_array, uptr debug_markers_index) {
	// Определяем величину, на размер которой может отличаться время цикла (safety_bytes). Когда мы просыпаемся чтобы писать звук,
	// смотрим где находится play_cursor и делаем прогноз где от будет находиться при смене кадра (expected_flip_play_cursor_unwrapped).
	// Если write_cursor + safety_bytes < expected_flip_play_cursor_unwrapped, то это значит что у нас звуковая карта с маленькой задержкой
	// и мы успеваем писать звук синхронно с изображением, поэтому пишем звук до конца следующего фрейма.
	// Если write_cursor + safety_bytes >= expected_flip_play_cursor_unwrapped, то полностью синхронизировать звук и изображение не получится,
	// просто пишем количество сэмплов, равное bytes_per_frame + safety_bytes

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

static void clear_sound_buffer(Sound& sound) {
	void *region_1, *region_2; DWORD region_1_size, region_2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(0, sound.get_buffer_size(), &region_1, &region_1_size, &region_2, &region_2_size, 0))) {
		return;
	}

	std::memset(region_1, 0, region_1_size);
	std::memset(region_2, 0, region_2_size);
	sound.buffer->Unlock(region_1, region_1_size, region_2, region_2_size);
}

static void fill_sound_buffer(const Game::Sound_Buffer& source, Sound& sound) {
	void *region_1, *region_2; DWORD region_1_size, region_2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.output_location, sound.output_byte_count, &region_1, &region_1_size, &region_2, &region_2_size, 0))) {
		return;
	}

	u32 sample_size = sizeof(*(source.samples));
	u32 region_1_size_samples = region_1_size / sample_size;
	u32 region_2_size_samples = region_2_size / sample_size;
	sound.running_sample_index += region_1_size_samples + region_2_size_samples;

	std::memcpy(region_1, source.samples, region_1_size);
	std::memcpy(region_2, source.samples + region_1_size_samples, region_2_size);
	sound.buffer->Unlock(region_1, region_1_size, region_2, region_2_size);
}

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

static u64 get_performance_frequency() {
	LARGE_INTEGER performance_frequency_result;
	QueryPerformanceFrequency(&performance_frequency_result);
	return (u64)performance_frequency_result.QuadPart;
}

namespace Platform {
	Read_File_Result read_file_sync(const char* file_name) {
		Read_File_Result result = {};
		u32 memory_size = 0;
		void* memory = nullptr;

		// похоже этот handle не надо закрывать
		HANDLE heap_handle = GetProcessHeap();
		if (!heap_handle) return result;

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		if (file_handle == INVALID_HANDLE_VALUE) return result;

		LARGE_INTEGER file_size;
		if (!GetFileSizeEx(file_handle, &file_size)) goto close_file_handle;

		memory_size = safe_truncate_to_u32(file_size.QuadPart);
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

namespace Debug {
	static void sound_sync_display(
		Screen& screen, const Sound& sound,
		const Marker* markers, uptr markers_count, uptr current_marker_index) {

		f32 horizontal_scaling = (f32)screen.get_width() / (f32)sound.get_buffer_size();
		Marker current_marker = markers[current_marker_index];

		u32 expected_flip_play_cursor_x = (u32)((f32)current_marker.expected_flip_play_cursor * horizontal_scaling);
		draw_vertical(screen, expected_flip_play_cursor_x, 0, screen.get_height(), 0xffffff00);

		for (uptr i = 0; i < markers_count; i++) {
			u32 top = 0;
			u32 bottom = screen.get_height() * 1/4;
			u32 historic_output_play_cursor_x = (u32)((f32)markers[i].output_play_cursor * horizontal_scaling);
			u32 historic_output_write_cursor_x = (u32)((f32)markers[i].output_write_cursor * horizontal_scaling);
			draw_vertical(screen, historic_output_play_cursor_x, top, bottom, 0x00ffffff);
			draw_vertical(screen, historic_output_write_cursor_x, top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.get_height() * 1/4;
			u32 bottom = screen.get_height() * 2/4;
			u32 output_play_cursor_x = (u32)((f32)current_marker.output_play_cursor * horizontal_scaling);
			u32 output_write_cursor_x = (u32)((f32)current_marker.output_write_cursor * horizontal_scaling);
			draw_vertical(screen, output_play_cursor_x, top, bottom, 0x00ffffff);
			draw_vertical(screen, output_write_cursor_x, top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.get_height() * 2/4;
			u32 bottom = screen.get_height() * 3/4;
			u32 output_location_x = (u32)((f32)current_marker.output_location * horizontal_scaling);
			u32 output_byte_count_x = (u32)((f32)current_marker.output_byte_count * horizontal_scaling);
			draw_vertical(screen, output_location_x, top, bottom, 0x00ffffff);
			draw_vertical(screen, (output_location_x + output_byte_count_x) % screen.get_width(), top, bottom, 0x00ff0000);
		}
		{
			u32 top = screen.get_height() * 3/4;
			u32 bottom = screen.get_height();
			u32 flip_play_cursor_x = (u32)((f32)current_marker.flip_play_cursor * horizontal_scaling);
			u32 safety_bytes_x = (u32)((f32)sound.safety_bytes * horizontal_scaling);
			draw_vertical(screen, (flip_play_cursor_x - safety_bytes_x / 2) % screen.get_width(), top, bottom, 0x00ffffff);
			draw_vertical(screen, (flip_play_cursor_x + safety_bytes_x / 2) % screen.get_width(), top, bottom, 0x00ffffff);
		}
	}
	
	static void draw_vertical(Screen& screen, u32 x, u32 top, u32 bottom, u32 color) {
		Game::Screen_Pixel* pixel = screen.memory + top * screen.get_width() + x;

		for (u32 y = top; y < bottom; y++) {
			// TODO: добавить u32 конструктор либо избавиться от ScreenPixel
			*pixel = *(Game::Screen_Pixel*)&color;
			pixel += screen.get_width();
		}
	}
}