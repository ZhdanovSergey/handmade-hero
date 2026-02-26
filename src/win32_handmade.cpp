#include "win32_handmade.hpp"

static Screen global_screen = Screen(); // глобальный из-за WindowProc

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	static_assert(DEV_MODE || !SLOW_MODE);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

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
	HWND window = CreateWindowExA(0, window_class.lpszClassName, "Handmade Hero", dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, adj_window_width, adj_window_height, nullptr, nullptr, hInstance, nullptr);
	
	void* game_memory_base_address = nullptr;
	if constexpr (DEV_MODE && UINTPTR_MAX == UINT64_MAX) game_memory_base_address = (void*)1024_GB;
	Game::Memory game_memory = {};
	game_memory.permanent_size = 64_MB;
	game_memory.transient_size = 1_GB;
	// TODO: использовать MEM_LARGE_PAGES и AdjustTokenPrivileges в 64-битном билде
	game_memory.permanent_storage = (u8*)VirtualAlloc(game_memory_base_address, (SIZE_T)game_memory.get_total_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	game_memory.transient_storage = game_memory.permanent_storage + game_memory.permanent_size;
	game_memory.read_file_sync = Platform::read_file_sync;
	game_memory.write_file_sync = Platform::write_file_sync;
	game_memory.free_file_memory = Platform::free_file_memory;

	Input input = Input();
	Sound sound = Sound(window);
	Game_Code game_code = Game_Code();
	Dev_Replayer dev_replayer = Dev_Replayer(game_memory);

	bool dev_is_pause = false;
	i64 flip_timestamp = get_timestamp();
	// u64 flip_cycle_counter = __rdtsc();

	while (true) {
		input.game_input.reset_counters();
		input.process_gamepad();
		input.dev_process_mouse(window);

		MSG message;
		while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
			switch (message.message) {
				case WM_KEYUP:
				case WM_KEYDOWN: {
					bool is_key_pressed  = !(message.lParam & (1U << 31));
					bool was_key_pressed =   message.lParam & (1U << 30);
					if (is_key_pressed == was_key_pressed) continue;
					input.process_keyboard_button(message.wParam, is_key_pressed);
					if constexpr (DEV_MODE) {
						if (!is_key_pressed) continue;
						if (message.wParam == 'P') dev_is_pause = !dev_is_pause;
						if (message.wParam == 'L') dev_replayer.next_state(game_memory, input.game_input);
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
			game_code.dev_reload_if_recompiled();
			if (dev_is_pause) {
				wait_until_end_of_frame(flip_timestamp);
				flip_timestamp = get_timestamp();
				continue;
			};
			dev_replayer.record_or_replace(game_memory, input.game_input);
		}

		game_code.update_and_render(input.game_input, game_memory, global_screen.game_screen);
		sound.calc_samples_to_write(flip_timestamp);
		game_code.get_sound_samples(game_memory, sound.game_sound);
		sound.submit();
		
		wait_until_end_of_frame(flip_timestamp);
		// char output_buffer[256];
		// sprintf_s(output_buffer, "frame ms: %.2f\n", get_seconds_elapsed(flip_timestamp) * 1000);
		// OutputDebugStringA(output_buffer);
		flip_timestamp = get_timestamp();

		// sound.dev_draw_sync(global_screen);
		HDC device_context = GetDC(window);
		global_screen.submit(window, device_context);
		ReleaseDC(window, device_context);
	}
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC device_context = BeginPaint(window, &paint);
			global_screen.submit(window, device_context);
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

static void get_build_file_path(const char* file_name, char* dest, i32 dest_size) {
	char build_file_path[MAX_PATH]; // TODO: путь может быть более длинным
	GetModuleFileNameA(nullptr, build_file_path, sizeof(build_file_path));
	char* one_past_last_slash = build_file_path;
	for (char* scan = build_file_path; *scan; scan++) {
		if (*scan == '\\') one_past_last_slash = scan + 1;
	}
	i32 build_folder_path_size = (i32)(one_past_last_slash - build_file_path);
	hm::strcat(
		build_file_path, build_folder_path_size,
		file_name, hm::strlen(file_name),
		dest, dest_size
	);

}

static FILETIME get_file_write_time(const char* file_name) {
	WIN32_FILE_ATTRIBUTE_DATA file_data;
	if (!GetFileAttributesExA(file_name, GetFileExInfoStandard, &file_data)) return {};
	return file_data.ftLastWriteTime;
}

static inline f32 get_seconds_elapsed(i64 start) {
	return (f32)(get_timestamp() - start) / (f32)PERFORMANCE_FREQUENCY;
}

static inline i64 get_timestamp() {
	LARGE_INTEGER performance_counter_result;
	QueryPerformanceCounter(&performance_counter_result);
	return performance_counter_result.QuadPart;
}

Game_Code::Game_Code() {
	auto& game_code = *this;
	memset(&game_code, 0, sizeof(game_code));
	game_code.update_and_render = [](auto...){};
	game_code.get_sound_samples = [](auto...){};
	get_build_file_path("game.dll", game_code.dll_path, sizeof(game_code.dll_path));
	get_build_file_path("game_temp.dll", game_code.temp_dll_path, sizeof(game_code.temp_dll_path));
	game_code.load();
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

	// загружаем копию, чтобы компилятор мог писать в оригинальный файл
	HMODULE loaded_dll = LoadLibraryA(game_code.temp_dll_path);
	if (!loaded_dll) return;

	game_code.dll = loaded_dll;
	game_code.write_time = get_file_write_time(game_code.dll_path);
	game_code.update_and_render = (Game::Update_And_Render*)GetProcAddress(loaded_dll, "update_and_render");
	game_code.get_sound_samples = (Game::Get_Sound_Samples*)GetProcAddress(loaded_dll, "get_sound_samples");
}

void Game_Code::dev_reload_if_recompiled() {
	if constexpr (!DEV_MODE) return;
	auto& game_code = *this;
	FILETIME dll_write_time = get_file_write_time(game_code.dll_path);
	if (!CompareFileTime(&dll_write_time, &game_code.write_time)) return;

	FreeLibrary(game_code.dll);
	game_code.dll = nullptr;
	game_code.update_and_render = [](auto...){};
	game_code.get_sound_samples = [](auto...){};
	game_code.load();
}

Input::Input() {
	auto& input = *this;
	memset(&input, 0, sizeof(input));
	input.XInputGetState = [](auto...){ return 1ul; };
	input.XInputSetState = [](auto...){ return 1ul; };
	input.game_input.frame_dt = TARGET_SECONDS_PER_FRAME;
	HMODULE xinput_dll = LoadLibraryA("xinput1_3.dll");
	if (!xinput_dll) return;
	
	input.XInputGetState = (Xinput_Get_State*)GetProcAddress(xinput_dll, "XInputGetState");
	input.XInputSetState = (Xinput_Set_State*)GetProcAddress(xinput_dll, "XInputSetState");
}

void Input::process_gamepad() {
	auto& input = *this;
	Game::Controller& controller = input.game_input.controllers[1];
	XINPUT_STATE state;
	if (input.XInputGetState(0, &state)) {
		controller.is_connected = false;
		return;
	};
	
	controller.is_connected = true;
	controller.start_x = controller.end_x;
	controller.start_y = controller.end_y;
	controller.end_x = get_normalized_stick_value(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	controller.end_y = get_normalized_stick_value(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
	controller.average_x = controller.min_x = controller.max_x = controller.end_x;
	controller.average_y = controller.min_y = controller.max_y = controller.end_y;
	controller.is_analog = controller.average_x || controller.average_y;

	process_gamepad_button(controller.start,			state.Gamepad.wButtons & XINPUT_GAMEPAD_START);
	process_gamepad_button(controller.back,				state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
	process_gamepad_button(controller.left_shoulder, 	state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
	process_gamepad_button(controller.right_shoulder,	state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);

	process_gamepad_button(controller.move_up, 			state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
	process_gamepad_button(controller.move_down, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
	process_gamepad_button(controller.move_left, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
	process_gamepad_button(controller.move_right, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

	process_gamepad_button(controller.action_up, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
	process_gamepad_button(controller.action_down, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_A);
	process_gamepad_button(controller.action_left, 		state.Gamepad.wButtons & XINPUT_GAMEPAD_X);
	process_gamepad_button(controller.action_right, 	state.Gamepad.wButtons & XINPUT_GAMEPAD_B);
}

f32 Input::get_normalized_stick_value(SHORT value, SHORT deadzone) {
	return (-deadzone <= value && value < 0) || (0 < value && value <= deadzone) ? 0 : value / ((f32)INT16_MAX + 1.0f);
}

void Input::process_gamepad_button(Game::Button& state, bool is_pressed) {
	state.transitions_count += is_pressed != state.is_pressed;
	state.is_pressed = is_pressed;
}

void Input::process_keyboard_button(WPARAM key_code, bool is_pressed) {
	auto& input = *this;
	Game::Controller& controller = input.game_input.controllers[0];
	controller.is_connected = true;
	Game::Button* button_state;
	switch (key_code) {
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
		default: return;
	}
	button_state->is_pressed = is_pressed;
	button_state->transitions_count++;
}

void Input::dev_process_mouse(HWND window) {
	if constexpr (!DEV_MODE) return;
	auto& input = *this;
	Game::Dev_Mouse& mouse = input.game_input.dev_mouse;

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

Dev_Replayer::Dev_Replayer(const Game::Memory& game_memory) {
	auto& replayer = *this;
	memset(&replayer, 0, sizeof(replayer));
	if constexpr (!DEV_MODE) return;

	get_build_file_path("replay_state.hms", replayer.state_path, sizeof(replayer.state_path));
	get_build_file_path("replay_input.hmi", replayer.input_path, sizeof(replayer.input_path));
	replayer.state_handle = CreateFileA(replayer.state_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_NO_BUFFERING, nullptr);
	replayer.input_handle = CreateFileA(replayer.input_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
}

void Dev_Replayer::next_state(Game::Memory& game_memory, Game::Input& game_input) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	replayer.replayer_state = (Dev_Replayer_State)((replayer.replayer_state + 1) % Dev_Replayer_State::Count);

	switch (replayer.replayer_state) {
		case Dev_Replayer_State::Recording: {
			replayer.start_record(game_memory);
		} break;
		case Dev_Replayer_State::Playing: {
			replayer.start_play(game_memory);
		} break;
		case Dev_Replayer_State::Idle: {
			game_input = {}; // принудительно отжимаем нажатые кнопки
		} break;
		case Dev_Replayer_State::Count: break;
	}
}

void Dev_Replayer::record_or_replace(Game::Memory& game_memory, Game::Input& game_input) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	switch (replayer.replayer_state) {
		case Dev_Replayer_State::Recording: {
			replayer.record(game_input);
		} break;
		case Dev_Replayer_State::Playing: {
			replayer.play(game_memory, game_input);
		} break;
		case Dev_Replayer_State::Idle: case Dev_Replayer_State::Count: break;
	}
}

void Dev_Replayer::start_record(const Game::Memory& game_memory) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	i64 state_total_size = game_memory.get_total_size();
	DWORD bytes_to_write = (DWORD)state_total_size;
	assert(bytes_to_write == state_total_size);
	DWORD bytes_written;
	WriteFile(replayer.state_handle, game_memory.permanent_storage, bytes_to_write, &bytes_written, nullptr);
}

void Dev_Replayer::start_play(Game::Memory& game_memory) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	SetFilePointer(replayer.state_handle, 0, 0, FILE_BEGIN);
	SetFilePointer(replayer.input_handle, 0, 0, FILE_BEGIN);
	i64 state_total_size = game_memory.get_total_size();
	DWORD bytes_to_read = (DWORD)state_total_size;
	assert(bytes_to_read == state_total_size);
	DWORD bytes_read;
	ReadFile(replayer.state_handle, game_memory.permanent_storage, bytes_to_read, &bytes_read, nullptr);
}

void Dev_Replayer::record(const Game::Input& game_input) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	DWORD bytes_written;
	WriteFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_written, nullptr);
}

void Dev_Replayer::play(Game::Memory& game_memory, Game::Input& game_input) {
	if constexpr (!DEV_MODE) return;
	auto& replayer = *this;
	DWORD bytes_read;
	ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	if (!bytes_read) {
		replayer.start_play(game_memory);
		ReadFile(replayer.input_handle, &game_input, sizeof(game_input), &bytes_read, nullptr);
	}
}

Screen::Screen() {
	auto& screen = *this;
	memset(&screen, 0, sizeof(screen));
	screen.bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	screen.bitmap_info.bmiHeader.biPlanes = 1;
	screen.bitmap_info.bmiHeader.biBitCount = sizeof(*screen.game_screen.pixels) * 8;
	screen.bitmap_info.bmiHeader.biCompression = BI_RGB;
	screen.resize(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT);
}

void Screen::resize(i32 width, i32 height) {
	auto& screen = *this;
	if (width == screen.game_screen.width && height == screen.game_screen.height) return;

	VirtualFree(screen.game_screen.pixels, 0, MEM_RELEASE);
	screen.bitmap_info.bmiHeader.biWidth = (LONG)width;
	screen.bitmap_info.bmiHeader.biHeight = - (LONG)height; // отрицательный чтобы верхний левый пиксель был первым в буфере
	screen.game_screen.width = width;
	screen.game_screen.height = height;
	SIZE_T memory_size = width * height * sizeof(*screen.game_screen.pixels);
	screen.game_screen.pixels = (u32*)VirtualAlloc(nullptr, memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void Screen::submit(HWND window, HDC device_context) const {
	auto& screen = *this;
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

void Screen::dev_draw_vertical(i32 x, i32 top, i32 bottom, u32 color) {
	if constexpr (!DEV_MODE) return;
	auto& screen = *this;
	u32* pixel = screen.game_screen.pixels + top * screen.game_screen.width + x;
	for (i32 y = top; y < bottom; y++) {
		*pixel = color;
		pixel += screen.game_screen.width;
	}
}

Sound::Sound(HWND window) {
	auto& sound = *this;
	memset(&sound, 0, sizeof(sound));
	sound.wave_format.wFormatTag = WAVE_FORMAT_PCM;
	sound.wave_format.nChannels = 2;
	sound.wave_format.nSamplesPerSec = 48'000;
	sound.wave_format.nBlockAlign = sizeof(Game::Sound_Sample);
	sound.wave_format.nAvgBytesPerSec = sound.wave_format.nBlockAlign * sound.wave_format.nSamplesPerSec;
	sound.wave_format.wBitsPerSample = sound.wave_format.nBlockAlign / sound.wave_format.nChannels * 8u;
	sound.game_sound.samples_per_second = (i32)sound.wave_format.nSamplesPerSec;
	sound.game_sound.samples = (Game::Sound_Sample*)VirtualAlloc(nullptr, sound.get_buffer_size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	HMODULE direct_sound_dll = LoadLibraryA("dsound.dll");
	if (!direct_sound_dll) return;

	Direct_Sound_Create* DirectSoundCreate = (Direct_Sound_Create*)GetProcAddress(direct_sound_dll, "DirectSoundCreate");
	IDirectSound* direct_sound;
	if (!SUCCEEDED(DirectSoundCreate(nullptr, &direct_sound, nullptr))) return;
	if (!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) return;

	DSBUFFERDESC primary_buffer_desc = {};
	primary_buffer_desc.dwSize = sizeof(primary_buffer_desc);
	primary_buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

	IDirectSoundBuffer* primary_buffer;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&primary_buffer_desc, &primary_buffer, nullptr))) return;
	if (!SUCCEEDED(primary_buffer->SetFormat(&sound.wave_format))) return;

	DSBUFFERDESC sound_buffer_desc = {};
	sound_buffer_desc.dwSize = sizeof(sound_buffer_desc);
	sound_buffer_desc.dwBufferBytes = sound.get_buffer_size();
	sound_buffer_desc.lpwfxFormat = &sound.wave_format;
	if (!SUCCEEDED(direct_sound->CreateSoundBuffer(&sound_buffer_desc, &sound.buffer, nullptr))) return;

	void *region1, *region2; DWORD region1_size, region2_size;
	if(!SUCCEEDED(sound.buffer->Lock(0, sound.get_buffer_size(), &region1, &region1_size, &region2, &region2_size, 0))) return;
	memset(region1, 0, region1_size);
	memset(region2, 0, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
	sound.buffer->Play(0, 0, DSBPLAY_LOOPING);
}

void Sound::calc_samples_to_write(i64 flip_timestamp) {
	// Определяем величину, на размер которой может отличаться время цикла (safety_bytes). Когда мы просыпаемся чтобы писать звук,
	// смотрим где находится play_cursor и делаем прогноз где от будет находиться при смене кадра (expected_flip_play_cursor_unwrapped).
	// Если write_cursor + safety_bytes < expected_flip_play_cursor_unwrapped, то это значит что у нас звуковая карта с маленькой задержкой
	// и мы успеваем писать звук синхронно с изображением, поэтому пишем звук до конца следующего фрейма.
	// Если write_cursor + safety_bytes >= expected_flip_play_cursor_unwrapped, то полностью синхронизировать звук и изображение не получится,
	// просто пишем количество сэмплов, равное bytes_per_frame + safety_bytes

	auto& sound = *this;
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

void Sound::submit() {
	auto& sound = *this;
	void *region1, *region2; DWORD region1_size, region2_size;
	if (!sound.buffer || !SUCCEEDED(sound.buffer->Lock(sound.output_location,
		(DWORD)(sound.game_sound.samples_to_write * sound.wave_format.nBlockAlign),
		&region1, &region1_size, &region2, &region2_size, 0))) {
		return;
	}

	sound.output_location = (sound.output_location + region1_size + region2_size) % get_buffer_size();
	memcpy(region1, sound.game_sound.samples, region1_size);
	memcpy(region2, (u8*)sound.game_sound.samples + region1_size, region2_size);
	sound.buffer->Unlock(region1, region1_size, region2, region2_size);
}

void Sound::dev_draw_sync(Screen& screen) {
	if constexpr (!DEV_MODE) return;
	auto& sound = *this;
	if (!sound.buffer) return;

	f32 horizontal_scaling = (f32)screen.game_screen.width / (f32)sound.get_buffer_size();
	for (i32 i = 0; i < hm::array_size(sound.dev_markers); i++) {
		i32 top = 0;
		i32 bottom = screen.game_screen.height * 1/4;
		i32 historic_output_play_cursor_x = (i32)((f32)sound.dev_markers[i].output_play_cursor * horizontal_scaling);
		i32 historic_output_write_cursor_x = (i32)((f32)sound.dev_markers[i].output_write_cursor * horizontal_scaling);
		screen.dev_draw_vertical(historic_output_play_cursor_x, top, bottom, 0xffffff);
		screen.dev_draw_vertical(historic_output_write_cursor_x, top, bottom, 0xff0000);
	}

	if (!sound.game_sound.samples_to_write || !SUCCEEDED(sound.buffer->GetCurrentPosition(&sound.dev_markers[sound.dev_markers_index].flip_play_cursor, nullptr))) {
		sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_size(sound.dev_markers);
		return;
	}

	Dev_Sound_Time_Marker current_marker = sound.dev_markers[sound.dev_markers_index];
	i32 expected_flip_play_cursor_x = (i32)((f32)current_marker.expected_flip_play_cursor * horizontal_scaling);
	screen.dev_draw_vertical(expected_flip_play_cursor_x, 0, screen.game_screen.height, 0xffff00);

	{
		i32 top 	= screen.game_screen.height * 1/4;
		i32 bottom  = screen.game_screen.height * 2/4;
		i32 output_play_cursor_x = (i32)((f32)current_marker.output_play_cursor * horizontal_scaling);
		i32 output_write_cursor_x = (i32)((f32)current_marker.output_write_cursor * horizontal_scaling);
		screen.dev_draw_vertical(output_play_cursor_x, top, bottom, 0xffffff);
		screen.dev_draw_vertical(output_write_cursor_x, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.height * 2/4;
		i32 bottom  = screen.game_screen.height * 3/4;
		i32 output_location_x = (i32)((f32)current_marker.output_location * horizontal_scaling);
		i32 output_byte_count_x = (i32)((f32)current_marker.output_byte_count * horizontal_scaling);
		screen.dev_draw_vertical(output_location_x, top, bottom, 0xffffff);
		screen.dev_draw_vertical((output_location_x + output_byte_count_x) % screen.game_screen.width, top, bottom, 0xff0000);
	}
	{
		i32 top 	= screen.game_screen.height * 3/4;
		i32 bottom  = screen.game_screen.height;
		i32 flip_play_cursor_x = (i32)((f32)current_marker.flip_play_cursor * horizontal_scaling);
		i32 safety_bytes_x = (i32)((f32)sound.get_safety_bytes() * horizontal_scaling);
		screen.dev_draw_vertical((flip_play_cursor_x - safety_bytes_x / 2) % screen.game_screen.width, top, bottom, 0xffffff);
		screen.dev_draw_vertical((flip_play_cursor_x + safety_bytes_x / 2) % screen.game_screen.width, top, bottom, 0xffffff);
	}
	sound.dev_markers_index = (sound.dev_markers_index + 1) % hm::array_size(sound.dev_markers);
}

namespace Platform {
	Read_File_Result read_file_sync(const char* file_name) {
		Read_File_Result result = {};
		HANDLE heap_handle = GetProcessHeap();
		result.memory = HeapAlloc(heap_handle, 0, (DWORD)result.memory_size);

		HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		LARGE_INTEGER file_size = {};
		GetFileSizeEx(file_handle, &file_size);
		result.memory_size = (i32)file_size.QuadPart;
		assert(result.memory_size == file_size.QuadPart);

		DWORD bytes_read;
		if (!ReadFile(file_handle, result.memory, (DWORD)result.memory_size, &bytes_read, nullptr) || (bytes_read != (DWORD)result.memory_size)) {
			HeapFree(heap_handle, 0, result.memory);
			result = {};
		}

		CloseHandle(file_handle);
		return result;
	}

	bool write_file_sync(const char* file_name, const void* memory, i32 memory_size) {
		HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
		DWORD bytes_written;
		bool is_success = WriteFile(file_handle, memory, (DWORD)memory_size, &bytes_written, nullptr) && (bytes_written == (DWORD)memory_size);
		CloseHandle(file_handle);
		return is_success;
	}
	
	void free_file_memory(void*& memory) {
		HANDLE heap_handle = GetProcessHeap();
		HeapFree(heap_handle, 0, memory);
		memory = nullptr;
	}
}