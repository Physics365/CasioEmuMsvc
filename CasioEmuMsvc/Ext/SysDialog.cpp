#include "SysDialog.h"
#ifdef _WIN32
#include <ShlObj.h>
#include <SysDialog.h>
#include <Windows.h>
#include <commdlg.h>

std::filesystem::path SystemDialogs::OpenFileDialog() {
	auto cwd = std::filesystem::current_path();
	OPENFILENAME ofn;
	TCHAR szFile[260] = {0};
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("All\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetOpenFileName(&ofn) == TRUE) {
		std::filesystem::current_path(cwd);
		return std::filesystem::path(ofn.lpstrFile);
	}
	std::filesystem::current_path(cwd);
	return {};
}

std::filesystem::path SystemDialogs::SaveFileDialog(std::string prefered_name) {
	auto cwd = std::filesystem::current_path();
	OPENFILENAME ofn;
	TCHAR szFile[260] = {0};
	MultiByteToWideChar(65001, 0, prefered_name.c_str(), prefered_name.size(), szFile, 260);
	// strcpy_s(szFile, prefered_name.c_str());
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = TEXT("All\0*.*\0");
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	if (GetSaveFileName(&ofn) == TRUE) {
		std::filesystem::current_path(cwd);
		return std::filesystem::path(ofn.lpstrFile);
	}
	std::filesystem::current_path(cwd);
	return {};
}

std::filesystem::path SystemDialogs::OpenFolderDialog() {
	auto cwd = std::filesystem::current_path();
	BROWSEINFO bi = {0};
	bi.lpszTitle = TEXT("Select Folder");
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	std::filesystem::current_path(cwd);
	if (pidl != 0) {
		TCHAR path[MAX_PATH];
		if (SHGetPathFromIDList(pidl, path)) {
			return std::filesystem::path(path);
		}
	}
	return {};
}

std::filesystem::path SystemDialogs::SaveFolderDialog() {
	return OpenFolderDialog(); // In Windows, folder dialogs are usually used for both opening and saving.
}
#elif defined(__linux__)
#include <cstdlib>
#include <iostream>

std::filesystem::path RunKDialog(const std::string& args) {
	std::string command = "kdialog " + args;
	FILE* pipe = popen(command.c_str(), "r");
	if (!pipe)
		return std::filesystem::path();

	char buffer[128];
	std::string result = "";
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result += buffer;
	}

	pclose(pipe);
	if (!result.empty() && result.back() == '\n') {
		result.pop_back();
	}
	return std::filesystem::path(result);
}

std::filesystem::path SystemDialogs::OpenFileDialog() {
	return RunKDialog("--getopenfilename");
}

std::filesystem::path SystemDialogs::SaveFileDialog() {
	return RunKDialog("--getsavefilename");
}

std::filesystem::path SystemDialogs::OpenFolderDialog() {
	return RunKDialog("--getexistingdirectory");
}

std::filesystem::path SystemDialogs::SaveFolderDialog() {
	return OpenFolderDialog();
}

#endif
