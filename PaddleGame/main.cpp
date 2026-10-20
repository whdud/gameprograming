#define NOMINMAX
#include <windows.h>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

// 공, 벽돌 구조체
struct Vec2
{
	float x, y;
};

struct Ball
{
	Vec2 pos;
	Vec2 vel;	// 속도 벡터 (px/s)
	float r;
};

struct Brick
{
	RECT rc;
	bool alive;
	bool item;	// 깨지면 공 하나 추가
};

enum class GameState { Menu, Playing };

static float Clamp(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

class MyApp
{
public:
	MyApp() {};
	~MyApp() { DiscardDeviceResource(); DiscardAppResource(); };
	bool Initialize(HINSTANCE hInstance);
	bool Update(float timeDelta);
	void Render();

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void OnResize();
	bool CreateAppResource();
	void DiscardAppResource();
	bool CreateDeviceResource();
	void DiscardDeviceResource();

	void StartGame();
	void EndGame(bool clear);
	void AddBall();
	void HitBricks(Ball& b);

private:
	HWND hWnd = NULL;

	// GDI 더블 버퍼링
	HDC hdcMem = NULL;
	HBITMAP hBitmapMem = NULL;
	HFONT hTitleFont = NULL;

	GameState state = GameState::Menu;
	std::vector<Ball> balls;
	std::vector<Brick> bricks;
	std::mt19937 rng{ std::random_device{}() };

	float paddleX = 320;	// 패들 중심 x
	const float paddleW = 100;
	const float paddleH = 14;
	const float ballSpeed = 350;

	int pendingBalls = 0;	// 아이템으로 추가될 공 개수
	int score = 0;
	int lastScore = -1;
	bool lastClear = false;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MyApp myApp;
	if (!myApp.Initialize(hInstance)) return 1;

	LARGE_INTEGER nPrevTime;
	LARGE_INTEGER nFrequency;
	QueryPerformanceFrequency(&nFrequency);
	QueryPerformanceCounter(&nPrevTime);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			LARGE_INTEGER nCurrentTime;
			QueryPerformanceCounter(&nCurrentTime);
			float timeDelta = (float)((double)(nCurrentTime.QuadPart - nPrevTime.QuadPart) / (double)(nFrequency.QuadPart));
			nPrevTime = nCurrentTime;

			if (!myApp.Update(timeDelta)) break;
			myApp.Render();
		}
	}
	return 0;
}

bool MyApp::Initialize(HINSTANCE hInstance)
{
	WNDCLASS wc = {};
	wc.lpfnWndProc = MyApp::WndProc;
	wc.cbWndExtra = sizeof(LONG_PTR);
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"MyApp";
	RegisterClass(&wc);

	hWnd = CreateWindow(L"MyApp", L"Paddle Game", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 656, 519, NULL, NULL, hInstance, this);
	if (!hWnd) return false;

	ShowWindow(hWnd, SW_SHOWNORMAL);
	UpdateWindow(hWnd);

	if (!CreateAppResource()) return false;
	return true;
}

LRESULT CALLBACK MyApp::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_CREATE)
	{
		CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
		MyApp* pMyApp = (MyApp*)pCreate->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pMyApp);
		pMyApp->hWnd = hWnd;	// CreateWindow가 리턴하기 전에 WM_SIZE가 먼저 오기 때문
		return 0;
	}

	MyApp* pMyApp = (MyApp*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!pMyApp)
	{
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_SIZE:
		pMyApp->OnResize();
		return 0;

	case WM_MOUSEMOVE:
		pMyApp->paddleX = (float)(short)LOWORD(lParam);
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_SPACE && pMyApp->state == GameState::Menu)
			pMyApp->StartGame();
		else if (wParam == VK_ESCAPE)
		{
			if (pMyApp->state == GameState::Playing) pMyApp->state = GameState::Menu;
			else DestroyWindow(hWnd);
		}
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void MyApp::OnResize()
{
	// 창 크기가 바뀌면 백버퍼도 다시 만든다
	DiscardAppResource();
	CreateAppResource();
}

bool MyApp::CreateAppResource()
{
	if (hdcMem) return true;

	RECT rc;
	GetClientRect(hWnd, &rc);
	if (rc.right == 0 || rc.bottom == 0) return true;	// 최소화 상태

	HDC hdc = GetDC(hWnd);
	hdcMem = CreateCompatibleDC(hdc);
	hBitmapMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
	ReleaseDC(hWnd, hdc);
	SelectObject(hdcMem, hBitmapMem);

	hTitleFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"맑은 고딕");

	return true;
}

void MyApp::DiscardAppResource()
{
	if (hTitleFont) { DeleteObject(hTitleFont); hTitleFont = NULL; }
	if (hBitmapMem) { DeleteObject(hBitmapMem); hBitmapMem = NULL; }
	if (hdcMem) { DeleteDC(hdcMem); hdcMem = NULL; }
}

bool MyApp::CreateDeviceResource()
{
	return true;
}

void MyApp::DiscardDeviceResource()
{
}

void MyApp::StartGame()
{
	RECT rc;
	GetClientRect(hWnd, &rc);

	// 벽돌 배치 5줄 x 10칸
	bricks.clear();
	const int rows = 5, cols = 10, gap = 4, top = 60, h = 20;
	int w = (rc.right - 40 - gap * (cols - 1)) / cols;
	std::uniform_int_distribution<int> itemDist(0, 9);
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < cols; c++)
		{
			Brick b;
			b.rc.left = 20 + c * (w + gap);
			b.rc.top = top + r * (h + gap);
			b.rc.right = b.rc.left + w;
			b.rc.bottom = b.rc.top + h;
			b.alive = true;
			b.item = (itemDist(rng) == 0);	// 10% 확률
			bricks.push_back(b);
		}
	}

	balls.clear();
	pendingBalls = 0;
	AddBall();
	score = 0;
	state = GameState::Playing;
}

void MyApp::EndGame(bool clear)
{
	lastScore = score;
	lastClear = clear;
	state = GameState::Menu;
}

void MyApp::AddBall()
{
	RECT rc;
	GetClientRect(hWnd, &rc);

	// 아래쪽에서 위로 30~150도 사이 랜덤 방향
	std::uniform_real_distribution<float> angleDist(30.f, 150.f);
	float rad = angleDist(rng) * 3.14159265f / 180.f;

	Ball b;
	b.r = 8;
	b.pos = { paddleX, rc.bottom - 60.f };
	b.vel = { cosf(rad) * ballSpeed, -sinf(rad) * ballSpeed };
	balls.push_back(b);
}

void MyApp::HitBricks(Ball& b)
{
	for (Brick& br : bricks)
	{
		if (!br.alive) continue;

		// 원과 사각형 충돌
		float cx = Clamp(b.pos.x, (float)br.rc.left, (float)br.rc.right);
		float cy = Clamp(b.pos.y, (float)br.rc.top, (float)br.rc.bottom);
		float dx = b.pos.x - cx, dy = b.pos.y - cy;
		if (dx * dx + dy * dy > b.r * b.r) continue;

		br.alive = false;
		score += 10;
		if (br.item) pendingBalls++;

		// 덜 겹친 축으로 튕긴다
		float overlapX = std::min(b.pos.x + b.r - br.rc.left, br.rc.right - (b.pos.x - b.r));
		float overlapY = std::min(b.pos.y + b.r - br.rc.top, br.rc.bottom - (b.pos.y - b.r));
		if (overlapX < overlapY) b.vel.x = -b.vel.x;
		else b.vel.y = -b.vel.y;
		return;	// 한 프레임에 하나만
	}
}

bool MyApp::Update(float timeDelta)
{
	if (state != GameState::Playing) return true;
	if (timeDelta > 0.05f) timeDelta = 0.05f;	// 창 드래그 등으로 멈췄을 때 튀는 것 방지

	RECT rc;
	GetClientRect(hWnd, &rc);
	float W = (float)rc.right, H = (float)rc.bottom;

	paddleX = Clamp(paddleX, paddleW / 2, W - paddleW / 2);
	float padTop = H - 40;

	for (Ball& b : balls)
	{
		b.pos.x += b.vel.x * timeDelta;
		b.pos.y += b.vel.y * timeDelta;

		// 좌우 벽은 x만, 천장은 y만 반전
		if (b.pos.x - b.r < 0) { b.pos.x = b.r; b.vel.x = fabsf(b.vel.x); }
		if (b.pos.x + b.r > W) { b.pos.x = W - b.r; b.vel.x = -fabsf(b.vel.x); }
		if (b.pos.y - b.r < 0) { b.pos.y = b.r; b.vel.y = fabsf(b.vel.y); }

		// 패들
		if (b.vel.y > 0 && b.pos.y + b.r >= padTop && b.pos.y - b.r <= padTop + paddleH &&
			b.pos.x >= paddleX - paddleW / 2 && b.pos.x <= paddleX + paddleW / 2)
		{
			b.pos.y = padTop - b.r;
			b.vel.y = -b.vel.y;
		}

		HitBricks(b);
	}

	// 루프 도중에 push_back 하면 참조가 깨지므로 여기서 추가
	for (; pendingBalls > 0; pendingBalls--) AddBall();

	// 아래로 빠진 공 제거
	balls.erase(std::remove_if(balls.begin(), balls.end(),
		[H](const Ball& b) { return b.pos.y - b.r > H; }), balls.end());

	if (balls.empty()) EndGame(false);
	else if (std::none_of(bricks.begin(), bricks.end(), [](const Brick& br) { return br.alive; })) EndGame(true);

	return true;
}

void MyApp::Render()
{
	if (!CreateDeviceResource()) return;
	if (!hdcMem) return;

	RECT rc;
	GetClientRect(hWnd, &rc);

	HGDIOBJ oldBrush = SelectObject(hdcMem, GetStockObject(DC_BRUSH));
	HGDIOBJ oldPen = SelectObject(hdcMem, GetStockObject(NULL_PEN));

	SetDCBrushColor(hdcMem, RGB(20, 24, 40));
	FillRect(hdcMem, &rc, (HBRUSH)GetStockObject(DC_BRUSH));

	SetBkMode(hdcMem, TRANSPARENT);
	SetTextColor(hdcMem, RGB(255, 255, 255));

	if (state == GameState::Playing)
	{
		for (const Brick& br : bricks)
		{
			if (!br.alive) continue;
			SetDCBrushColor(hdcMem, br.item ? RGB(255, 200, 40) : RGB(90, 160, 230));
			FillRect(hdcMem, &br.rc, (HBRUSH)GetStockObject(DC_BRUSH));
		}

		RECT pad = { (LONG)(paddleX - paddleW / 2), rc.bottom - 40, (LONG)(paddleX + paddleW / 2), rc.bottom - 40 + (LONG)paddleH };
		SetDCBrushColor(hdcMem, RGB(240, 240, 240));
		FillRect(hdcMem, &pad, (HBRUSH)GetStockObject(DC_BRUSH));

		SetDCBrushColor(hdcMem, RGB(255, 90, 90));
		for (const Ball& b : balls)
			Ellipse(hdcMem, (int)(b.pos.x - b.r), (int)(b.pos.y - b.r), (int)(b.pos.x + b.r), (int)(b.pos.y + b.r));

		WCHAR buf[64];
		wsprintf(buf, L"SCORE %d   BALL %d", score, (int)balls.size());
		TextOut(hdcMem, 10, 10, buf, lstrlen(buf));
	}
	else
	{
		RECT title = rc;
		title.bottom = rc.bottom / 2;
		HGDIOBJ oldFont = SelectObject(hdcMem, hTitleFont);
		DrawText(hdcMem, L"PADDLE GAME", -1, &title, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
		SelectObject(hdcMem, oldFont);

		WCHAR buf[128];
		if (lastScore < 0) wsprintf(buf, L"SPACE : 시작    ESC : 종료\n\n마우스로 패들을 움직이세요");
		else wsprintf(buf, L"%s  점수 %d\n\nSPACE : 다시 시작    ESC : 종료", lastClear ? L"CLEAR!" : L"GAME OVER", lastScore);
		RECT info = rc;
		info.top = rc.bottom / 2 + 30;
		DrawText(hdcMem, buf, -1, &info, DT_CENTER);
	}

	SelectObject(hdcMem, oldBrush);
	SelectObject(hdcMem, oldPen);

	// 버퍼 스와핑
	HDC hdc = GetDC(hWnd);
	BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
	ReleaseDC(hWnd, hdc);

	bool DeviceLost = false;
	if (DeviceLost) DiscardDeviceResource();
}
