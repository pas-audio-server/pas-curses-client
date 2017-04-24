/*  This file is part of pas.

	pas is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	pas is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with pas.  If not, see <http://www.gnu.org/licenses/>.
 */

/*  pas is Copyright 2017 by Perry Kivolowitz

	CURSES TESTBED - NOT IN PRODUCTION PAS
*/

#include <sstream>
#include <iomanip>
#include <string>
#include <vector>

#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <wchar.h>

WINDOW * help_window = nullptr;
WINDOW * background_window = nullptr;

PANEL * help_panel = nullptr;
PANEL * background_panel = nullptr;

using namespace std;

static vector<wstring> help_text;
static int widest_help_line = 0;

// std::string -> std::wstring

inline wstring S2W(string s)
{
	wstring ws;
	ws.assign(s.begin(), s.end());
	return ws;
}

/*
// std::wstring -> std::string
std::wstring ws(L"wstring");
std::string s;
s.assign(ws.begin(), ws.end());
*/
void FillWindow(WINDOW * w, wchar_t f)
{
	int lines, cols;
	cchar_t C;

	C.attr = A_NORMAL;
	C.chars[0] = f;
	C.chars[1] = L'\0';

	getmaxyx(w, lines, cols);
	for (int l = 1; l < lines - 1; l++) {
		for (int c = 1; c < cols - 1; c++) {
			wmove(w, l, c);
			wadd_wch(w, &C);
		}
	}
}

void InitializeHelpText()
{
	vector<wstring> * v = &help_text;

	v->push_back(S2W("+    next DAC"));
	v->push_back(L"-    Schönberg DAC");
	v->push_back(S2W("UP   scroll up"));
	v->push_back(S2W("DN   scroll down"));
	v->push_back(L"^B   Schönberg up");
	v->push_back(S2W("^F   screen down"));
	v->push_back(S2W("RET  queue"));
	v->push_back(S2W("^X   Schönberg"));
	v->push_back(S2W("^N   next"));
	v->push_back(S2W("^P   pause"));
	v->push_back(S2W("^R   Schönberg"));
	v->push_back(S2W("ESC  quit"));
	v->push_back(S2W("^H   this help"));

	for (auto it = v->begin(); it < v->end(); it++) {
		if ((int) it->size() > widest_help_line)
			widest_help_line = it->size();
	}
}

void MakeHelpWindow(WINDOW * w, PANEL * p)
{
	if (help_text.size() == 0)
		InitializeHelpText();

	int width, height;
	getmaxyx(w, height, width);
	werase(w);
	box(w, 0, 0);
	wmove(w, 1, 1);
	waddwstr(w, S2W("pas-curses-client help         any key to return").c_str());
	wmove(w, 2, 1);
	whline(w, ACS_HLINE, width - 2);

	int lines_of_text = height - 4;
	int columns_needed = (help_text.size() + lines_of_text - 1) / lines_of_text;

	if (columns_needed > 2) {
		wmove(w, height - 2, 1);
		waddwstr(w, S2W("need a taller window").c_str());
		return;
	}

	int counter = 0;
	for (auto it = help_text.begin(); it < help_text.end(); it++, counter++) {
		wmove(w, 3 + (counter % lines_of_text), 1 + (counter / lines_of_text) * width / 2);
		waddwstr(w, it->c_str());
	}

	hide_panel(p);
}


int main(int argc, char * argv[])
{
	setlocale(LC_ALL,"");	

	initscr();
	background_window = newwin(0, 0, 0, 0);
	background_panel = new_panel(background_window);
	wborder(background_window, 0, 0, 0, 0, 0, 0, 0, 0);

	help_window = newwin(14, 50, 0, 0);
	help_panel = new_panel(help_window);
	wborder(help_window, 0, 0, 0, 0, 0, 0, 0, 0);

	FillWindow(background_window, wchar_t('ö'));
	MakeHelpWindow(help_window, help_panel);

	move_panel(help_panel, 10, 10);
	update_panels();
	doupdate();

	wgetch(background_window);
	show_panel(help_panel);
	update_panels();
	doupdate();
	wgetch(help_window);
	hide_panel(help_panel);

	wgetch(background_window);
	endwin();
}
