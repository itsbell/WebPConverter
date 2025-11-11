
// WebPConverterDlg.h: 헤더 파일
//

#pragma once


// CWebPConverterDlg 대화 상자
class CWebPConverterDlg : public CDialogEx
{
// 생성입니다.
public:
	CWebPConverterDlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_WEBPCONVERTER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedBtnLoad();
	afx_msg void OnBnClickedBtnConvert();
private:
	int m_nLoadMode;
	int m_nJpegDecodeModule;
	CListBox m_listLog;
	float m_fQuality;
public:
	afx_msg void OnBnClickedBtnWebpConfigApply();
};
