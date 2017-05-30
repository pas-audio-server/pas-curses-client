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
 */

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <ctime>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <memory.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unordered_set>
#include <time.h>
#include <assert.h>
#include <ncursesw/curses.h>
#include <ncursesw/panel.h>
#include <wchar.h>

#include "commands.pb.h"

using namespace std;
using namespace pas;

ofstream _log_;

/*
// std::string -> std::wstring
std::string s("string");of
std::wstring ws;
ws.assign(s.begin(), s.end());

// std::wstring -> std::string
std::wstring ws(L"wstring");
std::string s;
s.assign(ws.begin(), ws.end());
*/

inline wstring S2W(string s)
{
	wstring ws;
	ws.assign(s.begin(), s.end());
	return ws;
}

#define	LOG(s)		(string(__FILE__) + string(":") + string(__FUNCTION__) + string("() line: ") + to_string(__LINE__) + string(" msg: ") + string(s))

map<char, int> jump_marks;

int current_dac_index = 0;
wstring dac_name;

bool keep_going = true;
bool curses_is_active = false;
int server_socket = -1;

WINDOW * top_window = nullptr;
WINDOW * mid_left = nullptr;
WINDOW * mid_right = nullptr;
WINDOW * bottom_window = nullptr;
WINDOW * help_window = nullptr;

PANEL * top_panel2 = nullptr;
PANEL * mid_left_panel = nullptr;
PANEL * mid_right_panel = nullptr;
PANEL * bottom_panel2 = nullptr;
PANEL * help_panel = nullptr;

static vector<wstring> help_text;
static int widest_help_line = 0;

struct Track
{
	wstring id;
	wstring title;
	wstring artist;
};

vector<Track> tracks;
int index_of_first_visible_track = 0;
int index_of_high_lighted_line = 0;
int number_of_dacs = 0;
int left_width = 32;
int right_width = 0;
int scroll_height = 0;
int top_window_height = 3;

inline int StartLineForBottomWindow()
{
	return LINES - (2 + number_of_dacs);
}

inline int MidWindowHeight()
{
	return LINES - number_of_dacs - 2 - top_window_height;
}

void SIGHandler(int signal_number)
{
	keep_going = false;
}

void InitializeHelpText()
{
	vector<wstring> * v = &help_text;

	v->push_back(L"+    next DAC");
	v->push_back(L"-    previous DAC");
	v->push_back(L"UP   scroll up");
	v->push_back(L"DN   scroll down");
	v->push_back(L"^B   screen up");
	v->push_back(L"^F   screen down");
	v->push_back(L"RET  queue");
	v->push_back(L"^X   stop");
	v->push_back(L"^N   next");
	v->push_back(L"^P   pause");
	v->push_back(L"^R   resume");
	v->push_back(L"ESC  quit");
	v->push_back(L"^H   this help");

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
	waddwstr(w, L"pas-curses-client help         any key to return");
	wmove(w, 2, 1);
	whline(w, ACS_HLINE, width - 2);

	int lines_of_text = height - 4;
	int columns_needed = (help_text.size() + lines_of_text - 1) / lines_of_text;

	if (columns_needed > 2) {
		wmove(w, height - 2, 1);
		waddwstr(w, (wchar_t *) "need a taller window");
		return;
	}

	int counter = 0;
	for (auto it = help_text.begin(); it < help_text.end(); it++, counter++) {
		wmove(w, 3 + (counter % lines_of_text), 1 + (counter / lines_of_text) * width / 2);
		waddwstr(w, it->c_str());
	}

	hide_panel(p);
}

/*	InitializeNetworkConnection() - This function provides standard network
	client initialization which does include accepting an alternate port
	and IP address.

	The default port and IP, BTW, are: 5077 and 127.0.0.1.

Parameters:
int argc		Taken from main - the usual meaning
char * argv[]	Taken from main - the usual meaning

Returns:
int				The file descriptor of the server socket

Side Effects:

Expceptions:

Error conditions are thrown as strings.
 */

int InitializeNetworkConnection(int argc, char * argv[])
{
	int server_socket = -1;
	int port = 5077;
	char * ip = (char *) ("127.0.0.1");
	int opt;

	while ((opt = getopt(argc, argv, "dh:p:")) != -1) 
	{
		switch (opt) 
		{
			case 'd':
				_log_.open("/tmp/curses_log.txt");
				break;

			case 'h':
				ip = optarg;
				break;

			case 'p':
				port = atoi(optarg);
				break;
		}
	}

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		throw LOG(string(strerror(errno)));
	}

	hostent * server_hostent = gethostbyname(ip);
	if (server_hostent == nullptr)
	{
		close(server_socket);
		throw  "Failed gethostbyname()";
	}

	sockaddr_in server_sockaddr;
	memset(&server_sockaddr, 0, sizeof(sockaddr_in));
	server_sockaddr.sin_family = AF_INET;
	memmove(&server_sockaddr.sin_addr.s_addr, server_hostent->h_addr, server_hostent->h_length);
	server_sockaddr.sin_port = htons(port);

	if (connect(server_socket, (struct sockaddr*) &server_sockaddr, sizeof(sockaddr_in)) == -1)
	{
		throw LOG(string(strerror(errno)));
	}

	return server_socket;
}

void SendPB(string & s, int server_socket)
{
	assert(server_socket >= 0);

	// NOTE:
	// NOTE: length CANNOT be a size_t as size_t can be 64 or 32 bits depending upon
	// NOTE: the machine. unsigned int is 32 bits. This difference in size can cause
	// NOTE: the received length to leave behind (or send extra) bytes.
	// NOTE:

	unsigned int length = s.size();
	unsigned int ll = length;

	length = htonl(length);
	size_t bytes_sent = send(server_socket, (const void *) &length, sizeof(length), 0);
	if (bytes_sent != sizeof(length))
		throw string("bad bytes_sent for length");

	bytes_sent = send(server_socket, (const void *) s.data(), ll, 0);
	if (bytes_sent != ll)
		throw string("bad bytes_sent for message");
}

string GetResponse(int server_socket, Type & type)
{
	// NOTE:
	// NOTE: length CANNOT be a size_t as size_t can be 64 or 32 bits depending upon
	// NOTE: the machine. unsigned int is 32 bits. This difference in size can cause
	// NOTE: the received length to leave behind (or send extra) bytes.
	// NOTE:

	unsigned int length = 0;
	size_t bytes_read = recv(server_socket, (void *) &length, sizeof(length), 0);
	if (bytes_read != sizeof(length))
		throw LOG("bad recv getting length: " + to_string(bytes_read));

	string s;
	length = ntohl(length);
	s.resize(length);

	bytes_read = recv(server_socket, (void *) &s[0], length, MSG_WAITALL);
	if (bytes_read != length)
		throw LOG("bad recv getting pbuffer: " + to_string(bytes_read));
	GenericPB g;
	type = GENERIC;
	if (g.ParseFromString(s))
		type = g.type();
	return s;
}

/*
void Sanitize(string & s)
{
	for (auto it = s.begin(); it < s.end(); it++)
	{
		if (*it & 0x80)
			*it = '@';
	}
}
*/

void FetchTracks()
{
	// NOTE:
	// NOTE: ADD A MORE SOPHISTICATED SELECT_QUERY THAT TAKES AN ORDER STRING
	// NOTE:

	string s;
	SelectQueryExpanded c;
	c.set_type(SELECT_QUERY_EXPANDED);
	c.set_column(string("artist"));
	c.set_pattern(string("%"));
	c.set_orderby(string("artist, album, disc, track * 1"));
	bool outcome = c.SerializeToString(&s);
	if (!outcome)
		throw string("bad serialize");
	SendPB(s, server_socket);
	Type type;
	s = GetResponse(server_socket, type);
	// Tracks will come sorted on artist.
	char last_letter = '*';

	if (type == Type::SELECT_RESULT)
	{
		SelectResult sr;
		if (!sr.ParseFromString(s))
		{
			throw string("parsefromstring failed");
		}
		for (int i = 0; i < sr.row_size(); i++)
		{
			Row r = sr.row(i);
			// r.results_size() tells how many are in map.
			google::protobuf::Map<string, string> m = r.results();
			Track t;
			
			t.id = S2W(m["id"]);
			t.artist = S2W(m["artist"]);
			t.title = S2W(m["title"]);

			if (t.title.size() == 0)
				continue;

			if (t.artist.size() == 0)
				continue;
			
			if (t.artist[0] != last_letter)
			{
				jump_marks[t.artist[0]] = (int) tracks.size();
				last_letter = t.artist[0];
			}
			
//			if (t.artist[0] == 'A' && t.artist[1] == '.')
//				wcout << t.artist << endl;

			tracks.push_back(t);
		}	
	}
}

void CurrentDACInfo()
{
	string s = "DAC: ";
	wstring ws;
	ws.assign(s.begin(), s.end());
	ws += to_wstring(current_dac_index);

	werase(top_window);
	wmove(top_window, 1, 1);
	waddwstr(top_window, ws.c_str());

	s = "Name: ";
	ws.assign(s.begin(), s.end());
	wmove(top_window, 1, 10);
	waddwstr(top_window, ws.c_str());
	waddwstr(top_window, dac_name.c_str());
	box(top_window, 0, 0);
}

int FindNumberOfDACs()
{
	int rv = -1;
	string s;
	DacInfo a;
	a.set_type(DAC_INFO_COMMAND);
	if (a.SerializeToString(&s))
	{
		SendPB(s, server_socket);
		Type type;
		s = GetResponse(server_socket, type);
		if (type == Type::SELECT_RESULT)
		{
			SelectResult sr;

			if (sr.ParseFromString(s))
				rv = sr.row_size();
		}
	}
	return rv;
}

void DACInfoCommand()
{
	assert(server_socket >= 0);
	string s;
	DacInfo a;
	a.set_type(DAC_INFO_COMMAND);
	bool outcome = a.SerializeToString(&s);
	if (!outcome)
		throw string("DacInfoCommand() bad serialize");

	SendPB(s, server_socket);
	Type type;
	_log_ << __FUNCTION__ << " " << __LINE__ << " " << "before" << endl;
	try {
		s = GetResponse(server_socket, type);
	}
	catch (string s) {
		return;
	}
	_log_ << __FUNCTION__ << " " << __LINE__ << " " << "after" << endl;
	werase(bottom_window);
	if (type == Type::SELECT_RESULT)
	{
		SelectResult sr;
		if (!sr.ParseFromString(s))
		{
			throw string("dacinfocomment parsefromstring failed");
		}
		wstring ws;
		for (int i = 0; i < sr.row_size(); i++)
		{
			Row r = sr.row(i);
			google::protobuf::Map<string, string> results = r.results();

			if (i == current_dac_index) {
				dac_name.assign(results[string("friendly_name")].begin(), results[string("friendly_name")].end());
				_log_ << __FUNCTION__ << " " << __LINE__ << endl;
				if (dac_name.length() == 0)
				{
					dac_name.assign(results[string("name")].begin(), results[string("name")].end());
					_log_ << __FUNCTION__ << " " << __LINE__ << endl;
				}
			}
			wmove(bottom_window, 1 + i, 1);
			ws.assign(results[string("index")].begin(), results[string("index")].end());
			waddwstr(bottom_window, ws.c_str());
			wmove(bottom_window, 1 + i, 5);
			ws.assign(results[string("who")].begin(), results[string("who")].end());			
			if (ws.size() > 26)
				ws.resize(26);
			waddwstr(bottom_window, ws.c_str());
			
			wmove(bottom_window, 1 + i, 32);
			ws.assign(results[string("what")].begin(), results[string("what")].end());
			int maxw = COLS - 10 - 32;		
			if (ws.size() > maxw)
				ws.resize(maxw);
			waddwstr(bottom_window, ws.c_str());

			ws.assign(results[string("when")].begin(), results[string("when")].end());			
			wmove(bottom_window, 1 + i, COLS - 9);
			waddwstr(bottom_window, ws.c_str());
		}
		box(bottom_window, 0, 0);
	}
	else
	{
		throw string("did not get select_result back");
	}
}

void DisplayTracks()
{
	werase(mid_left);
	werase(mid_right);

	for (int i = 0; i < scroll_height - 2; i++)
	{
		int index = i + index_of_first_visible_track;

		if (i + index_of_first_visible_track >= (int) tracks.size())
			index = index % tracks.size();

		if ((int) i == index_of_high_lighted_line)
		{
			wattron(mid_left, A_STANDOUT);
			wattron(mid_right, A_STANDOUT);
		}
		else
		{
			wattroff(mid_left, A_STANDOUT);
			wattroff(mid_right, A_STANDOUT);
		}

		wstring s = tracks.at(index).artist;

//		if (s[0] == 'A' && s[1] == '.')
//			wcout << s << endl;

		if ((int) s.size() > left_width - 2)
			s.resize(left_width -2);
		mvwaddwstr(mid_left, i + 1, 1, s.data());

		s = tracks.at(index).title;

		if ((int) s.size() > right_width - 2)
			s.resize(right_width -2);
		mvwaddwstr(mid_right, i + 1, 1, s.c_str());
	}
	wattroff(mid_left, A_STANDOUT);
	wattroff(mid_right, A_STANDOUT);
	box(mid_left, 0,0);
	box(mid_right, 0,0);
}

void TrackCount()
{
	string s = "Number of Tracks: ";
	wstring ws;
	ws.assign(s.begin(), s.end());
	ws = ws + to_wstring(tracks.size());
	wmove(top_window, 1, COLS - ws.size() - 1);
	waddwstr(top_window, ws.c_str());
}

void DevCmdNoReply(Type type, int server_socket)
{
	string s;
	ClearDeviceCommand c;
	c.set_type(type);
	c.set_device_id(current_dac_index);
	bool outcome = c.SerializeToString(&s);
	if (!outcome)
		throw string("bad serialize");
	SendPB(s, server_socket);
}

void UpdateAndRender(bool do_dac_info = false)
{
	right_width = COLS - left_width;
	wresize(mid_right, scroll_height, right_width);

	if (do_dac_info)
		DACInfoCommand();
	CurrentDACInfo();
	TrackCount();
	DisplayTracks();
	wmove(top_window, 1, 1);
	update_panels();
	doupdate();
}

inline void ChangeCurrentDAC(int offset)
{
	current_dac_index += offset;
	current_dac_index = (current_dac_index + number_of_dacs) % number_of_dacs;
}


void ChangeHighlightedLine(int offset)
{
	index_of_high_lighted_line += offset;

	if (offset < 0 && index_of_high_lighted_line < 0)
	{
		index_of_high_lighted_line = 0;
		index_of_first_visible_track--;
	}
	else if (offset > 0 && index_of_high_lighted_line >= scroll_height - 3 )
	{
		index_of_high_lighted_line = scroll_height - 3;
		index_of_first_visible_track++;
	}
	else {
		// Impossible else.
	}
	index_of_first_visible_track = (index_of_first_visible_track + tracks.size()) % tracks.size();
}

void Play()
{
	string s;
	wstring ws;
	PlayTrackCommand cmd;

	int index = index_of_high_lighted_line + index_of_first_visible_track;
	index %= tracks.size();
	ws = tracks.at(index).id;
	s.assign(ws.begin(), ws.end());
	int track = atoi(s.c_str());
	cmd.set_type(PLAY_TRACK_DEVICE);
	cmd.set_device_id(current_dac_index);
	cmd.set_track_id(track);

	if (!cmd.SerializeToString(&s))
		throw string("InnerPlayCommand() bad serialize");
	SendPB(s, server_socket);

}		

void ScrollByScreen(int offset)
{
	index_of_first_visible_track += offset * (scroll_height - 2);
	index_of_first_visible_track = (index_of_first_visible_track + tracks.size()) % tracks.size();
}

void ShowHelp()
{
	show_panel(help_panel);
	update_panels();
	doupdate();
	getch();
	hide_panel(help_panel);
}

int main(int argc, char * argv[])
{
	setlocale(LC_ALL,"");
	
	if (signal(SIGINT, SIGHandler) == SIG_ERR)
		throw LOG("");

	try {
		server_socket = InitializeNetworkConnection(argc, argv);
	}
	catch (string s) {
		if (s.size() > 0)
			cerr << s << endl;
		return 1;
	}

	cout << "Fetching tracks..." << endl;
	FetchTracks();
	cout << "Done." << endl;
	cout << "Determining number of DACS..." << endl;
	number_of_dacs = FindNumberOfDACs();
	cout << "There is / are " << number_of_dacs << " DAC(s)" << endl;

	initscr();
	right_width = COLS - left_width;
	noecho();
	cbreak();
	scroll_height = MidWindowHeight();
	top_window = newwin(top_window_height, COLS, 0, 0);
	mid_left = newwin(scroll_height, left_width, top_window_height, 0);
	mid_right = newwin(scroll_height, right_width, top_window_height, left_width);
	bottom_window = newwin(2 + number_of_dacs, COLS, StartLineForBottomWindow(), 0);

	help_window = newwin(12, 50, 0, 0);
	help_panel = new_panel(help_window);
	move_panel(help_panel, 1, 2);

	top_panel2 = new_panel(top_window);
	mid_left_panel = new_panel(mid_left);
	mid_right_panel = new_panel(mid_right);
	bottom_panel2 = new_panel(bottom_window);

	MakeHelpWindow(help_window, help_panel);

	nodelay(top_window, 1);
	keypad(top_window, 1);
	curs_set(0);

	curses_is_active = true;

	string e_string;
	time_t last_update = time(nullptr);

	box(top_window, 0, 0);
	box(bottom_window, 0, 0);
	box(mid_left, 0, 0);
	box(mid_right, 0, 0);

	try
	{

/*
		wmove(instruction_window, 0, 0);
		wattron(instruction_window, A_STANDOUT);
		waddwstr(instruction_window, "-/D- +/D+ <RET>/Q ^X/STP ^P/PSE ^R/RSM ^N/NXT ^L/CLR A-Z  UP/T- DN/T+ ^F/PG+ ^B/PG- ^C/ESC/EXIT");
		wattroff(instruction_window, A_STANDOUT);
		wmove(top_window, 0, 2);
*/

		string s;
		while (keep_going && curses_is_active)
		{
			bool display_needs_update = false;
			bool update_dac_info = false;

			int c = wgetch(top_window);
			if (c != ERR)
			{
				display_needs_update = true;
				switch (c)
				{
					case '+':
						ChangeCurrentDAC(1);
						break;

					case '-':
						ChangeCurrentDAC(-1);
						break;

					case KEY_UP:
						ChangeHighlightedLine(-1);
						break;

					case KEY_DOWN:
						ChangeHighlightedLine(1);
						break;

					case 2:
						// ^B
						ScrollByScreen(-1);
						break;

					case 6:
						// ^F
						ScrollByScreen(1);
						break;

					case 8:
						// ^H
						ShowHelp();
						break;

					case 12:
						// ^L 
						DevCmdNoReply(CLEAR_DEVICE, server_socket);
						break;

					case 14:
						// ^N
						DevCmdNoReply(NEXT_DEVICE, server_socket);
						break;

					case 16:
						// ^P
						DevCmdNoReply(PAUSE_DEVICE, server_socket);
						break;


					case 18:
						// ^R
						DevCmdNoReply(RESUME_DEVICE, server_socket);
						break;

					case 24:
						// ^X - control S wasn't working (probably S/Q).
						DevCmdNoReply(STOP_DEVICE, server_socket);
						break;

					case 10:
					case KEY_ENTER:
						Play();
						break;
		

					case 27:
						// ESC
						keep_going = false;
						break;

					default:
						display_needs_update = false;
						break;
				}
				if (isalnum(c))
				{
					display_needs_update = true;
					if (jump_marks.find((char) c) != jump_marks.end())
						index_of_first_visible_track = jump_marks[(char) c];
				}
			}
			if (!keep_going)
				break;

			if (difftime(time(nullptr), last_update) > 0.2) {
				last_update = time(nullptr);
				update_dac_info = true;
			}

			if (display_needs_update || update_dac_info) {
				UpdateAndRender(update_dac_info);
			}
			usleep(1000);
		}
	}
	catch (string s)
	{
		e_string = s;
	}

	if (curses_is_active)
	{
		curses_is_active = false;
		endwin();
	}

	if (server_socket >= 0)
		close(server_socket);

	if (e_string.size() > 0)
		cout << e_string << endl;

	if (_log_.is_open())
		_log_.close();

	return 0;
}

