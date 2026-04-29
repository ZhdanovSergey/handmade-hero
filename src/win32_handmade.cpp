#include "win32_handmade.hpp"

static Screen global_screen = init_screen(); // глобальный из-за WindowProc

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	HWND window = create_window(hInstance);
	Input input = init_input();
	Sound sound = init_sound(window);
	Game_Code game_code = init_game_code();
	Game::Memory game_memory = init_game_memory();
	Replayer replayer = {};
	if constexpr (DEV_MODE) replayer = init_replayer(game_memory);

	bool is_pause = false;
	i64 flip_timestamp = get_timestamp();
	// u64 flip_cycle_counter = __rdtsc();

	while (true) {
		reset_input_counters(input);
		process_gamepad_input(input);
		if constexpr (DEV_MODE) process_mouse_input(input, window);

		MSG message;
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
			switch (message.message) {
				case WM_KEYUP:
				case WM_KEYDOWN: {
					bool is_key_pressed  = !(message.lParam & (1U << 31));
					bool was_key_pressed =   message.lParam & (1U << 30);
					if (is_key_pressed == was_key_pressed) continue;
					process_keyboard_button(input, message.wParam, is_key_pressed);
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

		// if constexpr (DEV_MODE) draw_sound_sync(sound, global_screen);
		HDC device_context = GetDC(window);
		submit_screen(global_screen, window, device_context);
		ReleaseDC(window, device_context);
	}
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			submit_screen(global_screen, window, device_context);
			EndPaint(window, &paint);
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
		if (sleep_ms >= 1) Sleep((DWORD)sleep_ms);
	}
	flip_seconds_elapsed = get_seconds_elapsed(flip_timestamp);
	while (flip_seconds_elapsed < TARGET_SECONDS_PER_FRAME) {
		YieldProcessor();
		flip_seconds_elapsed = get_seconds_elapsed(flip_timestamp);
	}
}

static void get_build_file_path(span<const char> file_name, span<char> dest) {
	// TODO: обработать пути длиннее MAX_PATH
	char file_path_storage[MAX_PATH];
	span<char> file_path = file_path_storage;
	SetLastError(ERROR_SUCCESS);
	GetModuleFileNameA(nullptr, file_path.ptr, (DWORD)file_path.size);
	assert(GetLastError() != ERROR_INSUFFICIENT_BUFFER);
	span<char> folder_path = {
		file_path.ptr,
		hm::find_last_index(file_path, +[](char ch) { return ch == '\\'; }) + 1
	};
	hm::strcat(folder_path, file_name, dest);
}

static FILETIME get_file_write_time(const char* file_name) {
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	if (!GetFileAttributesExA(file_name, GetFileExInfoStandard, &file_data)) return {};
	return file_data.ftLastWriteTime;
}

static f32 get_seconds_elapsed(i64 start) {
	return (f32)(get_timestamp() - start) / (f32)PERF_FREQUENCY;
}

static f32 get_target_seconds_per_frame() {
	f32 target_frame_rate = 33.0f;
    HDC device_context = GetDC(0);
    f32 refresh_rate = (f32)GetDeviceCaps(device_context, VREFRESH);
    ReleaseDC(0, device_context);
	if (refresh_rate > 1.0f) {
		f32 sync_frame_rate = refresh_rate / hm::ceil(refresh_rate / target_frame_rate);
		if (sync_frame_rate >= 30.0f) target_frame_rate = sync_frame_rate;
	}
	return 1.0f / target_frame_rate;
}

static i64 get_perf_frequency() {
	LARGE_INTEGER query_result;
	QueryPerformanceFrequency(&query_result);
	return query_result.QuadPart;
}

static i64 get_timestamp() {
	LARGE_INTEGER performance_counter_result;
	QueryPerformanceCounter(&performance_counter_result);
	return performance_counter_result.QuadPart;
}

static Game_Code init_game_code() {
	Game_Code game_code = {};
	game_code.update_and_render = [](auto...){};
	game_code.get_sound_samples = [](auto...){};
	get_build_file_path("game.dll", game_code.dll_path);
	get_build_file_path("game_temp.dll", game_code.temp_dll_path);
	load_game_code(game_code);
	return game_code;
}

static Game::Memory init_game_memory() {
	Game::Memory game_memory = {};
	i64 permanent_size = 64_MB;
	i64 transient_size = 1_GB;
	void* base_address = nullptr;
	if constexpr (DEV_MODE && UINTPTR_MAX == UINT64_MAX) base_address = (void*)1024_GB;
	// TODO: использовать MEM_LARGE_PAGES и AdjustTokenPrivileges в 64-битном билде
	u8* game_storage = (u8*)VirtualAlloc(base_address, (SIZE_T)(permanent_size + transient_size), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	game_memory.permanent_storage = { game_storage, permanent_size };
	game_memory.transient_storage = { game_storage + permanent_size, transient_size };
	game_memory.read_file_sync = Platform::read_file_sync;
	game_memory.write_file_sync = Platform::write_file_sync;
	game_memory.free_file_memory = Platform::free_file_memory;
	return game_memory;
}

static void load_game_code(Game_Code& game_code) {
	BOOL copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);

	if constexpr (DEV_MODE) {
		while (!copy_result && GetLastError() == ERROR_SHARING_VIOLATION) {
			Sleep(1); // ждем в цикле когда Visual Studio освободит новый dll
			copy_result = CopyFileA(game_code.dll_path, game_code.temp_dll_path, FALSE);
		}
	}

	// загружаем копию, чтобы компилятор мог писать в оригинальный файл
	HMODULE loaded_dll = LoadLibraryA(game_code.temp_dll_path);
	if (loaded_dll) {
		game_code.dll = loaded_dll;
		game_code.write_time = get_file_write_time(game_code.dll_path);
		game_code.update_and_render = (Game::Update_And_Render*)GetProcAddress(loaded_dll, "update_and_render");
		game_code.get_sound_samples = (Game::Get_Sound_Samples*)GetProcAddress(loaded_dll, "get_sound_samples");
	}
}

static void reload_game_code_if_recompiled(Game_Code& game_code) {
	FILETIME dll_write_time = get_file_write_time(game_code.dll_path);
	if (CompareFileTime(&dll_write_time, &game_code.write_time)) {
		FreeLibrary(game_code.dll);
		game_code.dll = nullptr;
		game_code.update_and_render = [](auto...){};
		game_code.get_sound_samples = [](auto...){};
		load_game_code(game_code);
	}
}

static Input init_input() {
	Input input = {};
	input.XInputGetState = [](auto...){ return 1ul; };
	input.XInputSetState = [](auto...){ return 1ul; };
	input.game_input.frame_dt = TARGET_SECONDS_PER_FRAME;
	HMODULE xinput_dll = LoadLibraryA("xinput1_3.dll");
	if (xinput_dll) {
		input.XInputGetState = (Xinput_Get_State*)GetProcAddress(xinput_dll, "XInputGetState");
		input.XInputSetState = (Xinput_Set_State*)GetProcAddress(xinput_dll, "XInputSetState");
	}
	return input;
}

static void process_gamepad_input(Input& input) {
	Game::Controller& controller = input.game_input.controllers[1];
	XINPUT_STATE state;
	if (input.XInputGetState(0, &state)) {
		controller.is_connected = false;
		return;
	};
	
	controller.is_connected = true;
	controller.start_x = controller.end_x;
	controller.start_y = controller.end_y;
	controller.end_x = get_normalized_stick_value(state.Gamepad.sThumbLX);
	controller.end_y = get_normalized_stick_value(state.Gamepad.sThumbLY);
	controller.average_x = controller.min_x = controller.max_x = controller.end_x;
	controller.average_y = controller.min_y = controller.max_y = controller.end_y;
	controller.is_analog = controller.average_x || controller.average_y;

	process_gamepad_button(controller.start,		  state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
	process_gamepad_button(controller.back,			  state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	process_gamepad_button(controller.left_shoulder,  state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	process_gamepad_button(controller.right_shoulder, state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	process_gamepad_button(controller.move_up, 		  state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	process_gamepad_button(controller.move_down, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	process_gamepad_button(controller.move_left, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	process_gamepad_button(controller.move_right, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	process_gamepad_button(controller.action_up, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
	process_gamepad_button(controller.action_down, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
	process_gamepad_button(controller.action_left, 	  state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
	process_gamepad_button(controller.action_right,   state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
}

static f32 get_normalized_stick_value(SHORT value) {
	return value / 32768.0f * (value < - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || value > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
}

static void process_gamepad_button(Game::Button& state, bool is_pressed) {
	state.transitions_count += is_pressed != state.is_pressed;
	state.is_pressed = is_pressed;
}

static void process_keyboard_button(Input& input, WPARAM key_code, bool is_pressed) {
	Game::Controller& controller = input.game_input.controllers[0];
	controller.is_connected = true;
	Game::Button* button_state;
	switch (key_code) {
		case VK_RETURN: button_state = &controller.start;		   break;
		case VK_ESCAPE: button_state = &controller.back;		   break;
		case VK_UP: 	button_state = &controller.move_up;	       break;
		case VK_DOWN: 	button_state = &controller.move_down;	   break;
		case VK_LEFT: 	button_state = &controller.move_left;	   break;
		case VK_RIGHT: 	button_state = &controller.move_right;	   break;
		case 'W': 		button_state = &controller.action_up;	   break;
		case 'S': 		button_state = &controller.action_down;	   break;
		case 'A': 		button_state = &controller.action_left;	   break;
		case 'D': 		button_state = &controller.action_right;   break;
		case 'Q': 		button_state = &controller.left_shoulder;  break;
		case 'E': 		button_state = &controller.right_shoulder; break;
		default: return;
	}
	button_state->is_pressed = is_pressed;
	button_state->transitions_count++;
}

static void process_mouse_input(Input& input, HWND window) {
	Game::Mouse& mouse = input.game_input.mouse;

	POINT point;
	GetCursorPos(&point);
	ScreenToClient(window, &point);
	mouse.x = (i32)point.x;
	mouse.y = (i32)point.y;

	if (mouse.left_button.is_pressed != bool(GetKeyState(VK_LBUTTON) & (1 << 15))) {
		mouse.left_button.is_pressed = !mouse.left_button.is_pressed;
		mouse.left_button.transitions_count++;
	}

	if (mouse.right_button.is_pressed != bool(GetKeyState(VK_RBUTTON) & (1 << 15))) {
		mouse.right_button.is_pressed = !mouse.right_button.is_pressed;
		mouse.right_button.transitions_count++;
	}
}

static Replayer init_replayer(const Game::Memory& game_memory) {
	Replayer replayer = {};
	get_build_file_path("replay_state.hms", replayer.state_path);
	get_build_file_path("replay_input.hmi", replayer.input_path);
	replayer.state_handle = CreateFileA(replayer.state_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, nullptr);
	replayer.input_handle = CreateFileA(replayer.input_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	return replayer;
}

static void replayer_next_state(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	replayer.state = (Replayer_State)((replayer.state + 1) % Replayer_State::Count);

	switch (replayer.state) {
		case Replayer_State::Recording: {
			replayer_start_record(replayer, game_memory);
		} break;
		case Replayer_State::Playing: {
			replayer_start_play(replayer, game_memory);
		} break;
		case Replayer_State::Idle: {
			game_input = {}; // принудительно отжимаем нажатые кнопки
		} break;
		case Replayer_State::Count: break;
	}
}

static void replayer_record_or_replace(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	switch (replayer.state) {
		case Replayer_State::Recording: {
			replayer_record(replayer, game_input);
		} break;
		case Replayer_State::Playing: {
			replayer_play(replayer, game_memory, game_input);
		} break;
		case Replayer_State::Idle:
		case Replayer_State::Count: break;
	}
}

static void replayer_start_record(Replayer& replayer, const Game::Memory& game_memory) {
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	i64 state_total_size = game_memory.get_total_size();
	DWORD bytes_to_write = (DWORD)state_total_size;
	assert(bytes_to_write == state_total_size);
	DWORD bytes_written;
	WriteFile(replayer.state_handle, game_memory.permanent_storage.ptr, bytes_to_write, &bytes_written, nullptr);
}

static void replayer_start_play(Replayer& replayer, Game::Memory& game_memory) {
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	i64 state_total_size = game_memory.get_total_size();
	DWORD bytes_to_read = (DWORD)state_total_size;
	assert(bytes_to_read == state_total_size);
	DWORD bytes_read;
	ReadFile(replayer.state_handle, game_memory.permanent_storage.ptr, bytes_to_read, &bytes_read, nullptr);
}

static void replayer_record(Replayer& replayer, const Game::Input& game_input) {
	DWORD bytes_written;
	WriteFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_written, nullptr);
}

static void replayer_play(Replayer& replayer, Game::Memory& game_memory, Game::Input& game_input) {
	DWORD bytes_read;
	ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	if (!bytes_read) {
		replayer_start_play(replayer, game_memory);
		ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	}
}

static Screen init_screen() {
	Screen screen = {};
	screen.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	screen.bitmap_info.bmiHeader.biPlanes = 1;
	screen.bitmap_info.bmiHeader.biBitCount = sizeof(*screen.game_screen.pixels) * 8;
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
	VirtualFree(screen.game_screen.pixels, 0, MEM_RELEASE);
	screen.bitmap_info.bmiHeader.biWidth = (LONG)width;
	screen.bitmap_info.bmiHeader.biHeight = - (LONG)height; // отрицательный чтобы верхний левый пиксель был первым в буфере
	screen.game_screen.width = width;
	screen.game_screen.height = height;
	SIZE_T memory_size = width * height * sizeof(*screen.game_screen.pixels);
	screen.game_screen.pixels = (u32*)VirtualAlloc(nullptr, memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void submit_screen(const Screen& screen, HWND window, HDC device_context) {
	RECT client_rect;
	GetClientRect(window, &client_rect);

	int dest_width = client_rect.right - client_rect.left;
	int dest_height = client_rect.bottom - client_rect.top;
	int src_width = (int)screen.game_screen.width;
	int src_height = (int)screen.game_screen.height;

	// выводим пиксели 1 к 1 на время разработки рендерера
	dest_width = src_width;
	dest_height = src_height;

	StretchDIBits(device_context,
		0, 0, dest_width, dest_height,
		0, 0, src_width, src_height,
		screen.game_screen.pixels, &screen.bitmap_info,
		DIB_RGB_COLORS, SRCCOPY
	);
}

static void draw_vertical_line(Screen& screen, i32 x, i32 top, i32 bottom, u32 color) {
	u32* pixel = screen.game_screen.pixels + top * screen.game_screen.width + x;
	for (i32 y = top; y < bottom; y++) {
		*pixel = color;
		pixel += screen.game_screen.width;
	}
}

static Sound init_sound(HWND window) {
	Sound sound = {};
	sound.wave_format.wFormatTag = WAVE_FORMAT_PCM;
	sound.wave_format.nChannels = 2;
	sound.wave_format.nSamplesPerSec = 48'000;
	sound.wave_format.nBlockAlign = sizeof(Game::Sound_Sample);
	sound.wave_format.nAvgBytesPerSec = sound.wave_format.nBlockAlign * sound.wave_format.nSamplesPerSec;
	sound.wave_format.wBitsPerSample = sound.wave_format.nBlockAlign / sound.wave_format.nChannels * 8u;
	sound.game_sound.samples_per_second = (i32)sound.wave_format.nSamplesPerSec;
	sound.game_sound.samples = (Game::Sound_Sample*)VirtualAlloc(nullptr, sound.get_buffer_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	HMODULE direct_sound_dll = LoadLibraryA("dsound.dll");
	if (!direct_sound_dll) return sound;

	Direct_Sound_Create* DirectSoundCreate = (Direct_Sound_Create*)GetProcAddress(direct_sound_dll, "DirectSoundCreate");
	IDirectSound* direct_sound;
	if (!SUCCEEDED(DirectSoundCreate(nullptr, &direct_sound, nullptr))) return sound;
	if (!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return sound;

	DSBUFFERDESC primary_buffer_desc = {};
	primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
	primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	IDirectSoundBuffer* primary_buffer;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, nullptr))) return sound;
	if (!SUCCEEDED(primary_buffer->SetFormat(&sound.wave_format))) return sound;

	DSBUFFERDESC sound_buffer_desc = {};
	sound_buffer_desc.dwSize = sizeof(sound_buffer_desc);
	sound_buffer_desc.dwBufferBytes = sound.get_buffer_size();
	sound_buffer_desc.lpwfxFormat = &sound.wave_format;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr))) return sound;

	void *region1, *region2; DWORD region1_size, region2_size;
	if(!SUCCEEDED(sound.buffer->Lock(0, sound.get_buffer_size(), &region1, &region1_size, &region2, &region2_size, 0))) return sound;
	memset(region1, 0, region1_size);
	memset(region2, 0, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
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
	if (!sound.buffer || !SUCCEEDED(sound.buffer->GetCurrentPosition(&play_cursor, &write_cursor))) {
		sound.game_sound.samples_to_write = 0;
		sound.dev_markers[sound.dev_markers_index] = {};
		return;
	}

	f32 from_flip_to_audio_seconds = get_seconds_elapsed(flip_timestamp);
	DWORD expected_bytes_until_flip = sound.get_bytes_per_frame() - (DWORD)((f32)sound.wave_format.nAvgBytesPerSec * from_flip_to_audio_seconds);
	DWORD expected_flip_play_cursor_unwrapped = play_cursor + expected_bytes_until_flip;
	DWORD write_cursor_unwrapped = play_cursor < write_cursor
		? write_cursor
		: write_cursor + sound.get_buffer_size();

	bool is_low_latency_sound = (write_cursor_unwrapped + sound.get_safety_bytes()) < expected_flip_play_cursor_unwrapped;
	DWORD target_cursor_unwrapped = is_low_latency_sound
		? expected_flip_play_cursor_unwrapped + sound.get_bytes_per_frame()
		: write_cursor_unwrapped + sound.get_bytes_per_frame() + sound.get_safety_bytes();

	DWORD target_cursor = target_cursor_unwrapped % sound.get_buffer_size();
	DWORD output_byte_count = sound.output_location < target_cursor ? 0 : sound.get_buffer_size();
	output_byte_count += target_cursor - sound.output_location;
	sound.game_sound.samples_to_write = (i32)(output_byte_count / sound.wave_format.nBlockAlign);

	if constexpr (DEV_MODE) {
		sound.dev_markers[sound.dev_markers_index] = {};
		sound.dev_markers[sound.dev_markers_index].output_play_cursor = play_cursor;
		sound.dev_markers[sound.dev_markers_index].output_write_cursor = write_cursor;
		sound.dev_markers[sound.dev_markers_index].output_location = sound.output_location;
		sound.dev_markers[sound.dev_markers_index].output_byte_count = output_byte_count;
		sound.dev_markers[sound.dev_markers_index].expected_flip_play_cursor = expected_flip_play_cursor_unwrapped % sound.get_buffer_size();
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

static void submit_sound(Sound& sound) {
	void *region1, *region2; DWORD region1_size, region2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.output_location,
		(DWORD)(sound.game_sound.samples_to_write * sound.wave_format.nBlockAlign),
		&region1, &region1_size, &region2, &region2_size, 0))) {
		return;
	}

	sound.output_location = (sound.output_location + region1_size + region2_size) % sound.get_buffer_size();
	memcpy(region1, sound.game_sound.samples, region1_size);
	memcpy(region2, (u8*)sound.game_sound.samples + region1_size, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
}

static void draw_sound_sync(Sound& sound, Screen& screen) {
	if (!sound.buffer) return;

	f32 horizontal_scaling = (f32)screen.game_screen.width / (f32)sound.get_buffer_size();
	for (auto& marker : sound.dev_markers) {
		i32 top = 0;
		i32 bottom = screen.game_screen.height * 1/4;
		i32 historic_output_play_cursor_x = (i32)((f32)marker.output_play_cursor * horizontal_scaling);
		i32 historic_output_write_cursor_x = (i32)((f32)marker.output_write_cursor * horizontal_scaling);
		draw_vertical_line(screen, historic_output_play_cursor_x, top, bottom, 0xffffff);
		draw_vertical_line(screen, historic_output_write_cursor_x, top, bottom, 0xff0000);
	}

	if (!sound.game_sound.samples_to_write || !SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.dev_markers[sound.dev_markers_index].flip_play_cursor, nullptr))) {
		sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_size(sound.dev_markers);
		return;
	}

	Sound_Time_Marker current_marker = sound.dev_markers[sound.dev_markers_index];
	i32 expected_flip_play_cursor_x = (i32)((f32)current_marker.expected_flip_play_cursor * horizontal_scaling);
	draw_vertical_line(screen, expected_flip_play_cursor_x, 0, screen.game_screen.height, 0xffff00);

	{
		i32 top 	= screen.game_screen.height * 1/4;
		i32 bottom  = screen.game_screen.height * 2/4;
		i32 output_play_cursor_x = (i32)((f32)current_marker.output_play_cursor * horizontal_scaling);
		i32 output_write_cursor_x = (i32)((f32)current_marker.output_write_cursor * horizontal_scaling);
		draw_vertical_line(screen, output_play_cursor_x, top, bottom, 0xffffff);
		draw_vertical_line(screen, output_write_cursor_x, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.height * 2/4;
		i32 bottom  = screen.game_screen.height * 3/4;
		i32 output_location_x = (i32)((f32)current_marker.output_location * horizontal_scaling);
		i32 output_byte_count_x = (i32)((f32)current_marker.output_byte_count * horizontal_scaling);
		draw_vertical_line(screen, output_location_x, top, bottom, 0xffffff);
		draw_vertical_line(screen, (output_location_x + output_byte_count_x) % screen.game_screen.width, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.height * 3/4;
		i32 bottom  = screen.game_screen.height;
		i32 flip_play_cursor_x = (i32)((f32)current_marker.flip_play_cursor * horizontal_scaling);
		i32 safety_bytes_x = (i32)((f32)sound.get_safety_bytes() * horizontal_scaling);
		draw_vertical_line(screen, (flip_play_cursor_x - safety_bytes_x / 2) % screen.game_screen.width, top, bottom, 0xffffff);
		draw_vertical_line(screen, (flip_play_cursor_x + safety_bytes_x / 2) % screen.game_screen.width, top, bottom, 0xffffff);
	}
	sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_size(sound.dev_markers);
}

namespace Platform {
	span<u8> read_file_sync(const char* file_name) {
		span<u8> result = {};
		LARGE_INTEGER file_size = {};

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		GetFileSizeEx(file_handle, &file_size);
		result.size = file_size.QuadPart;
		assert((DWORD)result.size == (u64)result.size);

		HANDLE heap_handle = GetProcessHeap();
		result.ptr = (u8*)HeapAlloc(heap_handle, 0, (DWORD)result.size);

		DWORD bytes_read;
		if (!ReadFile(file_handle, result.ptr, (DWORD)result.size, &bytes_read, nullptr) || (bytes_read != (DWORD)result.size)) {
			HeapFree(heap_handle, 0, result.ptr);
			result = {};
		}

		CloseHandle(file_handle);
		return result;
	}

	bool write_file_sync(const char* file_name, span<const u8> file) {
		assert((DWORD)file.size == (u64)file.size);
		HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		DWORD bytes_written;
		bool is_success = WriteFile(file_handle, file.ptr, (DWORD)file.size, &bytes_written, nullptr) && (bytes_written == (DWORD)file.size);
		CloseHandle(file_handle);
		return is_success;
	}
	
	void free_file_memory(void*& memory) {
		HANDLE heap_handle = GetProcessHeap();
		HeapFree(heap_handle, 0, memory);
		memory = nullptr;
	}
}