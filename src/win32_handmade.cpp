#include "win32_handmade.hpp"

static Screen global_screen = create_screen(); // глобальный из-за WindowProc

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	HWND window = create_window(hInstance);
	auto input = create_input();
	auto sound = create_sound(window);
	auto game_code = create_game_code();
	auto game_memory = create_game_memory();
	auto replayer = create_replayer(game_memory);

	bool is_pause = false;
	i64 flip_timestamp = get_timestamp();
	// u64 flip_cycle_counter = __rdtsc();

	while (true) {
		reset_input_counters(input);
		collect_gamepad_input(input);
		if constexpr (DEV_MODE) {
			collect_mouse_input(input, window);
		}

		MSG message;
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
			switch (message.message) {
				case WM_KEYUP:
				case WM_KEYDOWN: {
					bool is_key_pressed  = !(message.lParam & (1U << 31));
					bool was_key_pressed =   message.lParam & (1U << 30);
					if (is_key_pressed == was_key_pressed) continue;

					collect_keyboard_button_input(input, message.wParam, is_key_pressed);
					
					if constexpr (DEV_MODE) {
						if (!is_key_pressed) continue;
						if (message.wParam == 'P') is_pause = !is_pause;
						if (message.wParam == 'R') replayer_next_state(replayer, game_memory, input.game_input);
					}
				} break;
				case WM_QUIT: {
					return (int)message.wParam;
				} break;
				default: {
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}
			}
		}

		if constexpr (DEV_MODE) {
			reload_game_code_if_recompiled(game_code);
			if (is_pause) {
				wait_until_end_of_frame(flip_timestamp);
				flip_timestamp = get_timestamp();
				continue;
			};
			replayer_record_or_replace(replayer, game_memory, input.game_input);
		}

		game_code.update_and_render(input.game_input, game_memory, global_screen.game_screen);
		calc_sound_samples_to_write(sound, flip_timestamp);
		game_code.get_sound_samples(game_memory, sound.game_sound);
		submit_sound(sound);
		
		wait_until_end_of_frame(flip_timestamp);
		// char output_buffer[256];
		// sprintf_s(output_buffer, "frame ms: %.2f\n", get_seconds_elapsed(flip_timestamp) * 1000);
		// OutputDebugStringA(output_buffer);
		flip_timestamp = get_timestamp();

		// if constexpr (DEV_MODE) draw_sound_sync(global_screen, sound);
		HDC device_context = GetDC(window);
		defer(ReleaseDC(window, device_context));
		submit_screen(global_screen, window, device_context);
	}
}

static HWND create_window(HINSTANCE hInstance) {
	WNDCLASSA window_class = {};
	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = "Handmade_Hero";
	RegisterClassA(&window_class);

	RECT window_rect = {};
	window_rect.right = INITIAL_WINDOW_WIDTH;
	window_rect.bottom = INITIAL_WINDOW_HEIGHT;
	DWORD dwStyle = WS_TILEDWINDOW | WS_VISIBLE;
	AdjustWindowRectEx(&window_rect, dwStyle, FALSE, 0);

	int adj_window_width = window_rect.right - window_rect.left;
	int adj_window_height = window_rect.bottom - window_rect.top;
	return CreateWindowExA(0, window_class.lpszClassName, "Handmade Hero", dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, adj_window_width, adj_window_height, nullptr, nullptr, hInstance, nullptr);
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			defer(EndPaint(window, &paint));
			submit_screen(global_screen, window, device_context);
		} break;
		case WM_DESTROY: {
			PostQuitMessage(0);
		} break;
		default:
			return DefWindowProcA(window, message, wParam, lParam);
	}

	return 0;
}

static void wait_until_end_of_frame(i64 flip_timestamp) {
	f32 flip_seconds_elapsed = get_seconds_elapsed(flip_timestamp);
	if (SLEEP_GRANULARITY_SECONDS) {
		f32 sleep_ms = 1000.0f * (TARGET_SECONDS_PER_FRAME - SLEEP_GRANULARITY_SECONDS - flip_seconds_elapsed);
		if (sleep_ms >= 1) Sleep(cast<DWORD>(sleep_ms));
	}
	flip_seconds_elapsed = get_seconds_elapsed(flip_timestamp);
	while (flip_seconds_elapsed < TARGET_SECONDS_PER_FRAME) {
		YieldProcessor();
		flip_seconds_elapsed = get_seconds_elapsed(flip_timestamp);
	}
}

static void get_build_file_path(const char* file_name, char* result, DWORD result_size) {
	SetLastError(0);
	GetModuleFileNameA(nullptr, result, result_size);
	if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
		// LATER: обработать пути длиннее MAX_PATH
		assert(false);
		return;
	}

	i64 folder_path_size = strrchr(result, '\\') + 1 - result;
	assert(folder_path_size >= 0 && folder_path_size < result_size);
	result[folder_path_size] = 0;

	if (strcat_s(result, result_size, file_name) == ERANGE) {
		// LATER: обработать пути длиннее MAX_PATH
		assert(false);
		return;
	}
}

static FILETIME get_file_write_time(const char* file_name) {
	WIN32_FILE_ATTRIBUTE_DATA file_data = {};
	GetFileAttributesExA(file_name, GetFileExInfoStandard, &file_data);
	return file_data.ftLastWriteTime;
}

static f32 get_seconds_elapsed(i64 start) {
	return cast<f32>(get_timestamp() - start) / cast<f32>(PERF_FREQUENCY);
}

static f32 get_target_seconds_per_frame() {
	f32 target_fps = 30.0f;

	// синхронизация с частотой монитора
	// f32 min_fps = 30.0f;
    // HDC device_context = GetDC(0);
	// defer(ReleaseDC(0, device_context));
    // f32 refresh_rate = cast<f32>(GetDeviceCaps(device_context, VREFRESH));
	// if (refresh_rate > 1.0f) {
	// 	f32 sync_fps = refresh_rate / hm::ceil(refresh_rate / target_fps);
	// 	if (sync_fps >= min_fps) target_fps = sync_fps;
	// }

	return 1.0f / target_fps;
}

static i64 get_perf_frequency() {
	LARGE_INTEGER query_result = {};
	QueryPerformanceFrequency(&query_result);
	return query_result.QuadPart;
}

static i64 get_timestamp() {
	LARGE_INTEGER performance_counter_result = {};
	QueryPerformanceCounter(&performance_counter_result);
	return performance_counter_result.QuadPart;
}

static Game::Memory create_game_memory() {
	i64 permanent_size = 64_MB;
	i64 transient_size = 1_GB;
	void* base_address = DEV_MODE && UINTPTR_MAX == UINT64_MAX ? (void*)1024_GB : nullptr;
	// LATER: проверить эффект использования MEM_LARGE_PAGES и AdjustTokenPrivileges в 64-битном билде
	u8* game_storage = cast<u8*>(VirtualAlloc(base_address, cast<SIZE_T>(permanent_size + transient_size), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	Game::Memory game_memory = {};
	game_memory.permanent        = { game_storage,                  permanent_size };
	game_memory.transient        = { game_storage + permanent_size, transient_size };
	game_memory.read_file_sync   = Platform::read_file_sync;
	game_memory.write_file_sync  = Platform::write_file_sync;
	game_memory.free_file_memory = Platform::free_file_memory;
	return game_memory;
}

static Game_Code create_game_code() {
	Game_Code game_code = {};
	get_build_file_path("game.dll",      game_code.dll_path,      sizeof(game_code.dll_path));
	get_build_file_path("game_temp.dll", game_code.temp_dll_path, sizeof(game_code.temp_dll_path));
	load_game_code(game_code);
	return game_code;
}

static void reload_game_code_if_recompiled(Game_Code& game_code) {
	FILETIME dll_write_time = get_file_write_time(game_code.dll_path);
	if (CompareFileTime(&dll_write_time, &game_code.write_time)) {
		FreeLibrary(game_code.dll);
		game_code.dll = nullptr;
		load_game_code(game_code);
	}
}

static void load_game_code(Game_Code& game_code) {
	char* dll_path = game_code.dll_path;
	if constexpr (DEV_MODE) {
		// загружаем копию чтобы компилятор мог писать в оригинальный файл
		// и если dll занят ждем в цикле когда Visual Studio его освободит
		BOOL is_copy_success = FALSE;
		do {
			SetLastError(0);
			is_copy_success = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);
		} while (!is_copy_success && GetLastError() == ERROR_SHARING_VIOLATION && (Sleep(1), true));
		assert(is_copy_success);
		dll_path = game_code.temp_dll_path;
	}

	HMODULE loaded_dll = LoadLibraryA(dll_path);
	if (loaded_dll) {
		game_code.dll = loaded_dll;
		game_code.write_time = get_file_write_time(game_code.dll_path);
		game_code.update_and_render = cast<Game::Update_And_Render*>(GetProcAddress(loaded_dll, "update_and_render"));
		game_code.get_sound_samples = cast<Game::Get_Sound_Samples*>(GetProcAddress(loaded_dll, "get_sound_samples"));
	} else {
		game_code.update_and_render = [](auto...){};
		game_code.get_sound_samples = [](auto...){};
	}
}

static Input create_input() {
	Input input = {};
	input.game_input.frame_dt = TARGET_SECONDS_PER_FRAME;
	HMODULE xinput_dll = LoadLibraryA("xinput1_3.dll");
	if (xinput_dll) {
		input.XInputGetState = cast<Xinput_Get_State*>(GetProcAddress(xinput_dll, "XInputGetState"));
		input.XInputSetState = cast<Xinput_Set_State*>(GetProcAddress(xinput_dll, "XInputSetState"));
	} else {
		input.XInputGetState = [](auto...){ return 1ul; };
		input.XInputSetState = [](auto...){ return 1ul; };
	}
	return input;
}

static void collect_gamepad_input(Input& input) {
	auto& controller = input.game_input.controllers[1];

	XINPUT_STATE xinput_state;
	controller.is_connected = !input.XInputGetState(0, &xinput_state);
	if (!controller.is_connected) return;

	controller.start_x = controller.end_x;
	controller.start_y = controller.end_y;
	controller.end_x = get_normalized_gamepad_stick_value(xinput_state.Gamepad.sThumbLX);
	controller.end_y = get_normalized_gamepad_stick_value(xinput_state.Gamepad.sThumbLY);

	controller.min_x = min(controller.start_x, controller.end_x);
	controller.min_y = min(controller.start_y, controller.end_y);
	controller.max_x = max(controller.start_x, controller.end_x);
	controller.max_y = max(controller.start_y, controller.end_y);
	controller.average_x = (controller.start_x + controller.end_x) / 2;
	controller.average_y = (controller.start_y + controller.end_y) / 2;

	controller.is_analog = controller.start_x || controller.start_y || controller.end_x || controller.end_y;
	if (controller.is_analog) {
		process_gamepad_button_input(controller.move_left,  controller.average_x < 0);
		process_gamepad_button_input(controller.move_right, controller.average_x > 0);
		process_gamepad_button_input(controller.move_down,  controller.average_y < 0);
		process_gamepad_button_input(controller.move_up,    controller.average_y > 0);
	} else {
		process_gamepad_button_input(controller.move_left, 	xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
		process_gamepad_button_input(controller.move_right, xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
		process_gamepad_button_input(controller.move_down, 	xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
		process_gamepad_button_input(controller.move_up, 	xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	}

	process_gamepad_button_input(controller.start,		    xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
	process_gamepad_button_input(controller.back,			xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	process_gamepad_button_input(controller.left_shoulder,  xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	process_gamepad_button_input(controller.right_shoulder, xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	process_gamepad_button_input(controller.action_up, 	    xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
	process_gamepad_button_input(controller.action_down,    xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
	process_gamepad_button_input(controller.action_left,    xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
	process_gamepad_button_input(controller.action_right,   xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
}

static f32 get_normalized_gamepad_stick_value(SHORT value) {
	bool is_outside_deadzone = hm::abs(value) > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	return is_outside_deadzone * value / cast<f32>(- INT16_MIN);
}

static void process_gamepad_button_input(Game::Controller_Button& state, bool is_pressed) {
	state.transitions_count += is_pressed != state.is_pressed;
	state.is_pressed = is_pressed;
}

static void collect_keyboard_button_input(Input& input, WPARAM key_code, bool is_pressed) {
	auto& controller = input.game_input.controllers[0];
	controller.is_connected = true;

	Game::Controller_Button* button = nullptr;
	switch (key_code) {
		case VK_RETURN: button = &controller.start;		     break;
		case VK_ESCAPE: button = &controller.back;		     break;
		case VK_UP: 	button = &controller.move_up;	     break;
		case VK_DOWN: 	button = &controller.move_down;	     break;
		case VK_LEFT: 	button = &controller.move_left;	     break;
		case VK_RIGHT: 	button = &controller.move_right;     break;
		case 'W': 		button = &controller.action_up;	     break;
		case 'S': 		button = &controller.action_down;    break;
		case 'A': 		button = &controller.action_left;    break;
		case 'D': 		button = &controller.action_right;   break;
		case 'Q': 		button = &controller.left_shoulder;  break;
		case 'E': 		button = &controller.right_shoulder; break;
		default: return;
	}

	button->is_pressed = is_pressed;
	button->transitions_count += 1;
}

static void collect_mouse_input(Input& input, HWND window) {
	POINT point = {};
	GetCursorPos(&point);
	ScreenToClient(window, &point);

	auto& mouse = input.game_input.mouse;
	mouse.x = point.x;
	mouse.y = point.y;

	if (mouse.left_button.is_pressed != bool(GetKeyState(VK_LBUTTON) & (1 << 15))) {
		mouse.left_button.is_pressed = !mouse.left_button.is_pressed;
		mouse.left_button.transitions_count += 1;
	}
	if (mouse.right_button.is_pressed != bool(GetKeyState(VK_RBUTTON) & (1 << 15))) {
		mouse.right_button.is_pressed = !mouse.right_button.is_pressed;
		mouse.right_button.transitions_count += 1;
	}
}

static Replayer create_replayer(const Game::Memory& game_memory) {
	if constexpr (!DEV_MODE) return {};

	Replayer replayer = {};
	get_build_file_path("replay_state.hms", replayer.state_path, sizeof(replayer.state_path));
	get_build_file_path("replay_input.hmi", replayer.input_path, sizeof(replayer.input_path));
	replayer.state_handle = CreateFileA(replayer.state_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, nullptr);
	replayer.input_handle = CreateFileA(replayer.input_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	return replayer;
}

static void replayer_next_state(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	switch (replayer.state) {
		case Replayer_State::Idle:      replayer.state = Replayer_State::Recording; break;
		case Replayer_State::Recording: replayer.state = Replayer_State::Playing;   break;
		case Replayer_State::Playing:   replayer.state = Replayer_State::Idle;      break;
	}

	switch (replayer.state) {
		case Replayer_State::Idle: {
			// принудительно отжимаем нажатые кнопки
			game_input.mouse = {};
			for (auto& controller : game_input.controllers) {
				controller = {};
			}
		}                                                                             break;
		case Replayer_State::Recording: replayer_start_record(replayer, game_memory); break;
		case Replayer_State::Playing:   replayer_start_play(replayer, game_memory);   break;
	}
}

static void replayer_record_or_replace(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	switch (replayer.state) {
		case Replayer_State::Idle:                                                        break;
		case Replayer_State::Recording: replayer_record(replayer, game_input);            break;
		case Replayer_State::Playing:   replayer_play(replayer, game_memory, game_input); break;
	}
}

static void replayer_start_record(Replayer& replayer, const Game::Memory& game_memory) {
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	DWORD game_memory_size = cast<DWORD>(game_memory.permanent.get_size() + game_memory.transient.get_size());
	DWORD bytes_written;
	WriteFile(replayer.state_handle, game_memory.permanent.base, game_memory_size, &bytes_written, nullptr);
}

static void replayer_record(Replayer& replayer, const Game::Input& game_input) {
	DWORD bytes_written;
	WriteFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_written, nullptr);
}

static void replayer_start_play(Replayer& replayer, Game::Memory& game_memory) {
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	DWORD game_memory_size = cast<DWORD>(game_memory.permanent.get_size() + game_memory.transient.get_size());
	DWORD bytes_read;
	ReadFile(replayer.state_handle, game_memory.permanent.base, game_memory_size, &bytes_read, nullptr);
	assert(bytes_read == game_memory_size);
}

static void replayer_play(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	DWORD bytes_read;
	ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	if (!bytes_read) {
		replayer_start_play(replayer, game_memory);
		ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	}
	assert(bytes_read == sizeof(game_input));
}

static Screen create_screen() {
	Screen screen = {};
	screen.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	screen.bitmap_info.bmiHeader.biPlanes = 1;
	screen.bitmap_info.bmiHeader.biBitCount = sizeof(*screen.game_screen.base) * 8;
	screen.bitmap_info.bmiHeader.biCompression = BI_RGB;	
	resize_screen(screen, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
	return screen;
}

static void reset_input_counters(Input& input) {
	input.game_input.mouse.left_button.transitions_count = 0;
	input.game_input.mouse.right_button.transitions_count = 0;

	for (auto& controller : input.game_input.controllers) {
		controller.start.transitions_count = 0;
		controller.back.transitions_count = 0;
		controller.left_shoulder.transitions_count = 0;
		controller.right_shoulder.transitions_count = 0;

		controller.move_up.transitions_count = 0;
		controller.move_down.transitions_count = 0;
		controller.move_left.transitions_count = 0;
		controller.move_right.transitions_count = 0;

		controller.action_up.transitions_count = 0;
		controller.action_down.transitions_count = 0;
		controller.action_left.transitions_count = 0;
		controller.action_right.transitions_count = 0;
	}
}

static void resize_screen(Screen& screen, i32 width, i32 height) {
	if (screen.game_screen.base) {
		VirtualFree(screen.game_screen.base, 0, MEM_RELEASE);
	}

	screen.bitmap_info.bmiHeader.biWidth = width;
	screen.bitmap_info.bmiHeader.biHeight = - height; // отрицательный чтобы верхний левый пиксель был первым в буфере

	screen.game_screen.count_x = width;
	screen.game_screen.count_y = height;
	SIZE_T memory_size = cast<SIZE_T>(screen.game_screen.get_size());
	screen.game_screen.base = cast<u32*>(VirtualAlloc(nullptr, memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
}

static void submit_screen(const Screen& screen, HWND window, HDC device_context) {
	RECT client_rect = {};
	GetClientRect(window, &client_rect);

	int src_width   = screen.game_screen.count_x;
	int src_height  = screen.game_screen.count_y;
	int dest_width  = client_rect.right  - client_rect.left;
	int dest_height = client_rect.bottom - client_rect.top;

	// выводим пиксели 1 к 1 на время разработки рендерера
	dest_width = src_width;
	dest_height = src_height;

	StretchDIBits(device_context,
		0, 0, dest_width, dest_height,
		0, 0, src_width, src_height,
		screen.game_screen.base, &screen.bitmap_info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

static void draw_vertical_line(Screen& screen, i32 x, i32 top, i32 bottom, u32 color) {
	u32* pixel = screen.game_screen.base + top * screen.game_screen.count_x + x;
	for (i32 y = top; y < bottom; ++y) {
		*pixel = color;
		pixel += screen.game_screen.count_x;
	}
}

static Sound create_sound(HWND window) {
	Sound sound = {};
	sound.wave_format.wFormatTag = WAVE_FORMAT_PCM;
	sound.wave_format.nChannels = 2;
	sound.wave_format.nSamplesPerSec = 48'000;
	sound.wave_format.nBlockAlign = sizeof(Game::Sound_Sample);
	sound.wave_format.nAvgBytesPerSec = sound.wave_format.nBlockAlign * sound.wave_format.nSamplesPerSec;
	sound.wave_format.wBitsPerSample = sound.wave_format.nBlockAlign / sound.wave_format.nChannels * 8u;

	sound.buffer_size = sound.wave_format.nAvgBytesPerSec;
	sound.bytes_per_frame = cast<DWORD>(cast<f32>(sound.wave_format.nAvgBytesPerSec) * TARGET_SECONDS_PER_FRAME);
	sound.safety_bytes = sound.bytes_per_frame / 3;

	sound.game_sound.samples_per_second = cast<i32>(sound.wave_format.nSamplesPerSec);
	sound.game_sound.samples.base = cast<Game::Sound_Sample*>(VirtualAlloc(nullptr, sound.buffer_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

	HMODULE direct_sound_dll = LoadLibraryA("dsound.dll");
	if (!direct_sound_dll) return {};

	auto* DirectSoundCreate = cast<Direct_Sound_Create*>(GetProcAddress(direct_sound_dll, "DirectSoundCreate"));
	if (!DirectSoundCreate) return {};

	IDirectSound* direct_sound;
	if (DirectSoundCreate(nullptr, &direct_sound, nullptr) != DS_OK) return {};
	if (direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY) != DS_OK) return {};

	DSBUFFERDESC primary_buffer_desc = {};
	primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
	primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	IDirectSoundBuffer* primary_buffer;
	if (direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, nullptr) != DS_OK) return {};
	if (primary_buffer->SetFormat(&sound.wave_format) != DS_OK) return {};

	DSBUFFERDESC sound_buffer_desc = {};
	sound_buffer_desc.dwSize = sizeof(sound_buffer_desc);
	sound_buffer_desc.dwBufferBytes = sound.buffer_size;
	sound_buffer_desc.lpwfxFormat = &sound.wave_format;
	if (direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr) != DS_OK) return {};

	void *region1, *region2; DWORD region1_size, region2_size;
	if(sound.buffer->Lock(0, sound.buffer_size, &region1, &region1_size, &region2, &region2_size, 0) == DS_OK) {
		memset(region1, 0, region1_size);
		memset(region2, 0, region2_size);
		sound.buffer->Unlock(region1, region1_size, region2, region2_size);
	}

	if (sound.buffer->Play(0, 0, DSBPLAY_LOOPING) != DS_OK) return {};
	sound.is_valid = true;
	return sound;
}

static void calc_sound_samples_to_write(Sound& sound, i64 flip_timestamp) {
	// Определяем величину, на размер которой может отличаться время цикла (safety_bytes). Когда мы просыпаемся чтобы писать звук,
	// смотрим где находится play_cursor и делаем прогноз где от будет находиться при смене кадра (expected_flip_play_cursor_unwrapped).
	// Если write_cursor + safety_bytes < expected_flip_play_cursor_unwrapped, то это значит что у нас звуковая карта с маленькой задержкой
	// и мы успеваем писать звук синхронно с изображением, поэтому пишем звук до конца следующего фрейма.
	// Если write_cursor + safety_bytes >= expected_flip_play_cursor_unwrapped, то полностью синхронизировать звук и изображение не получится,
	// просто пишем количество сэмплов, равное bytes_per_frame + safety_bytes

	DWORD play_cursor = 0, write_cursor = 0;
	if (!sound.is_valid || sound.buffer->GetCurrentPosition(&play_cursor, &write_cursor) != DS_OK) {
		sound.game_sound.samples.count = 0;
		sound.dev_markers[sound.dev_markers_index] = {};
		return;
	}

	f32 from_flip_to_audio_seconds = get_seconds_elapsed(flip_timestamp);
	DWORD expected_bytes_until_flip = sound.bytes_per_frame - cast<DWORD>(cast<f32>(sound.wave_format.nAvgBytesPerSec) * from_flip_to_audio_seconds);
	DWORD expected_flip_play_cursor_unwrapped = play_cursor + expected_bytes_until_flip;
	DWORD write_cursor_unwrapped = play_cursor < write_cursor
		? write_cursor
		: write_cursor + sound.buffer_size;

	bool is_low_latency_sound = (write_cursor_unwrapped + sound.safety_bytes) < expected_flip_play_cursor_unwrapped;
	DWORD target_cursor_unwrapped = is_low_latency_sound
		? expected_flip_play_cursor_unwrapped + sound.bytes_per_frame
		: write_cursor_unwrapped + sound.bytes_per_frame + sound.safety_bytes;

	DWORD target_cursor = target_cursor_unwrapped % sound.buffer_size;
	DWORD output_byte_count = target_cursor - sound.output_location;
	if (target_cursor < sound.output_location) output_byte_count += sound.buffer_size;
	sound.game_sound.samples.count = cast<i64>(output_byte_count / sizeof(Game::Sound_Sample));

	if constexpr (DEV_MODE) {
		sound.dev_markers[sound.dev_markers_index] = {};
		sound.dev_markers[sound.dev_markers_index].output_play_cursor = play_cursor;
		sound.dev_markers[sound.dev_markers_index].output_write_cursor = write_cursor;
		sound.dev_markers[sound.dev_markers_index].output_location = sound.output_location;
		sound.dev_markers[sound.dev_markers_index].output_byte_count = output_byte_count;
		sound.dev_markers[sound.dev_markers_index].expected_flip_play_cursor = expected_flip_play_cursor_unwrapped % sound.buffer_size;
	}
}

static void submit_sound(Sound& sound) {
	if (!sound.is_valid) return;

	DWORD bytes_to_lock = cast<DWORD>(sound.game_sound.samples.get_size());
	void *region1, *region2; DWORD region1_size, region2_size;
	if (sound.buffer->Lock(sound.output_location, bytes_to_lock, &region1, &region1_size, &region2, &region2_size, 0) == DS_OK) {
		sound.output_location = (sound.output_location + region1_size + region2_size) % sound.buffer_size;
		memcpy(region1, sound.game_sound.samples.base, region1_size);
		memcpy(region2, cast<u8*>(sound.game_sound.samples.base) + region1_size, region2_size);
		sound.buffer->Unlock(region1, region1_size, region2, region2_size);
	}
}

static void draw_sound_sync(Screen& screen, Sound& sound) {
	if (!sound.is_valid) return;

	f32 horizontal_scaling = cast<f32>(screen.game_screen.count_x) / cast<f32>(sound.buffer_size);
	for (auto& marker : sound.dev_markers) {
		i32 top = 0;
		i32 bottom = screen.game_screen.count_y * 1/4;
		i32 historic_output_play_cursor_x  = cast<i32>(cast<f32>(marker.output_play_cursor)  * horizontal_scaling);
		i32 historic_output_write_cursor_x = cast<i32>(cast<f32>(marker.output_write_cursor) * horizontal_scaling);
		draw_vertical_line(screen, historic_output_play_cursor_x,  top, bottom, 0xffffff);
		draw_vertical_line(screen, historic_output_write_cursor_x, top, bottom, 0xff0000);
	}

	if (!sound.game_sound.samples.count ||
		sound.buffer->GetCurrentPosition(&sound.dev_markers[sound.dev_markers_index].flip_play_cursor, nullptr) != DS_OK) {
		sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_count(sound.dev_markers);
		return;
	}

	auto current_marker = sound.dev_markers[sound.dev_markers_index];
	i32 expected_flip_play_cursor_x = cast<i32>(cast<f32>(current_marker.expected_flip_play_cursor) * horizontal_scaling);
	draw_vertical_line(screen, expected_flip_play_cursor_x, 0, screen.game_screen.count_y, 0xffff00);

	{
		i32 top 	= screen.game_screen.count_y * 1/4;
		i32 bottom  = screen.game_screen.count_y * 2/4;
		i32 output_play_cursor_x  = cast<i32>(cast<f32>(current_marker.output_play_cursor)  * horizontal_scaling);
		i32 output_write_cursor_x = cast<i32>(cast<f32>(current_marker.output_write_cursor) * horizontal_scaling);
		draw_vertical_line(screen, output_play_cursor_x,  top, bottom, 0xffffff);
		draw_vertical_line(screen, output_write_cursor_x, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.count_y * 2/4;
		i32 bottom  = screen.game_screen.count_y * 3/4;
		i32 output_location_x   = (i32)(cast<f32>(current_marker.output_location)   * horizontal_scaling);
		i32 output_byte_count_x = (i32)(cast<f32>(current_marker.output_byte_count) * horizontal_scaling);
		draw_vertical_line(screen, output_location_x, top, bottom, 0xffffff);
		draw_vertical_line(screen, (output_location_x + output_byte_count_x) % screen.game_screen.count_x, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.count_y * 3/4;
		i32 bottom  = screen.game_screen.count_y;
		i32 flip_play_cursor_x = cast<i32>(cast<f32>(current_marker.flip_play_cursor) * horizontal_scaling);
		i32 safety_bytes_x     = cast<i32>(cast<f32>(sound.safety_bytes)              * horizontal_scaling);
		draw_vertical_line(screen, (flip_play_cursor_x - safety_bytes_x / 2) % screen.game_screen.count_x, top, bottom, 0xffffff);
		draw_vertical_line(screen, (flip_play_cursor_x + safety_bytes_x / 2) % screen.game_screen.count_x, top, bottom, 0xffffff);
	}
	
	sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_count(sound.dev_markers);
}

namespace Platform {
	slice1<u8> read_file_sync(const char* file_name) {
		slice1<u8> result = {};

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		defer(CloseHandle(file_handle));

		LARGE_INTEGER file_size_struct = {};
		GetFileSizeEx(file_handle, &file_size_struct);
		DWORD file_size_casted = cast<DWORD>(file_size_struct.QuadPart);
		result.set_size(file_size_casted);

		HANDLE heap_handle = GetProcessHeap();
		result.base = cast<u8*>(HeapAlloc(heap_handle, 0, file_size_casted));

		DWORD bytes_read;
		if (!ReadFile(file_handle, result.base, file_size_casted, &bytes_read, nullptr)) {
			assert(false);
			HeapFree(heap_handle, 0, result.base);
			return {};
		}		
		return result;
	}

	void write_file_sync(const char* file_name, slice1<const u8> file) {
		HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		defer(CloseHandle(file_handle));
		DWORD bytes_written;
		WriteFile(file_handle, file.base, cast<DWORD>(file.get_size()), &bytes_written, nullptr);
	}
	
	void free_file_memory(void*& memory) {
		HANDLE heap_handle = GetProcessHeap();
		HeapFree(heap_handle, 0, memory);
		memory = nullptr;
	}
}