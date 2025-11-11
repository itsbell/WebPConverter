
// WebPConverterDlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "WebPConverter.h"
#include "WebPConverterDlg.h"
#include "afxdialogex.h"
#include "ConvertManager.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CWebPConverterDlg 대화 상자



CWebPConverterDlg::CWebPConverterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_WEBPCONVERTER_DIALOG, pParent)
	, m_nLoadMode(FALSE)
	, m_fQuality(0)
	, m_nJpegDecodeModule(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CWebPConverterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_LOAD_FILE, m_nLoadMode);
	DDX_Control(pDX, IDC_LIST_LOG, m_listLog);
	DDX_Text(pDX, IDC_EDIT_WEBP_CONFIG_QUALITY, m_fQuality);
	DDV_MinMaxInt(pDX, m_fQuality, 0, 100);
	DDX_Radio(pDX, IDC_RADIO_DECODE_CPU, m_nJpegDecodeModule);
}

BEGIN_MESSAGE_MAP(CWebPConverterDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BTN_LOAD, &CWebPConverterDlg::OnBnClickedBtnLoad)
	ON_BN_CLICKED(IDC_BTN_CONVERT, &CWebPConverterDlg::OnBnClickedBtnConvert)
	ON_BN_CLICKED(IDC_BTN_WEBP_CONFIG_APPLY, &CWebPConverterDlg::OnBnClickedBtnWebpConfigApply)
	ON_BN_CLICKED(IDC_RADIO_DECODE_TURBO_JPEG, &CWebPConverterDlg::OnBnClickedRadioDecodeTurboJpeg)
	ON_BN_CLICKED(IDC_RADIO_DECODE_NV_JPEG, &CWebPConverterDlg::OnBnClickedRadioDecodeNvJpeg)
END_MESSAGE_MAP()


// CWebPConverterDlg 메시지 처리기

BOOL CWebPConverterDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	// TODO: 여기에 추가 초기화 작업을 추가합니다.

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CWebPConverterDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	CDialogEx::OnSysCommand(nID, lParam);
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CWebPConverterDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CWebPConverterDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CWebPConverterDlg::OnBnClickedBtnLoad()
{
	UpdateData(TRUE);
	CONVERT_MGR->Load((LOAD_MODE)m_nLoadMode);
}

void CWebPConverterDlg::OnBnClickedBtnConvert()
{
	CONVERT_MGR->Convert();
}

void CWebPConverterDlg::OnBnClickedBtnWebpConfigApply()
{
	UpdateData(TRUE);
	CONVERT_MGR->SetQuality(m_fQuality);
}

void CWebPConverterDlg::OnBnClickedRadioDecodeTurboJpeg()
{
	CONVERT_MGR->SetJpegDecodeModule(TURBO_JPEG);
}

void CWebPConverterDlg::OnBnClickedRadioDecodeNvJpeg()
{
	CONVERT_MGR->SetJpegDecodeModule(NV_JPEG);
}
