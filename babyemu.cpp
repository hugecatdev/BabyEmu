#include <iostream>
#include <cstdint>
#include <vector>
#include <iomanip>
#include <map>
#include <chrono>
#include <sstream>

using namespace std;

typedef uint16_t babyword_t;

const size_t BABYMEM_SIZE = 32;

map<string, babyword_t> opcodes = {
	{"LDA", 0x0},
	{"STA", 0x1},
	{"ADD", 0xA},
	{"MULT", 0xB},
	{"JMP", 0x4},
	{"JNP", 0xD},
	{"IPRT", 0xC},
	{"NEG", 0x3},
	{"1DIV", 0x6},
	{"HLT", 0xF}
};

vector<string> splitLines(const string& text) {
	vector<string> lines;
	string current;

	for (char c : text) {
		if (c == '\n') {
			lines.push_back(current);
			current.clear();
		}
		else {
			current += c;
		}
	}

	if (!current.empty())
		lines.push_back(current);

	return lines;
}

string trim(const string& s) {
	size_t start = 0;
	while (start < s.size() && isspace(s[start])) start++;

	size_t end = s.size();
	while (end > start && isspace(s[end - 1])) end--;

	return s.substr(start, end - start);
}

vector<string> split(const string& line) {
	vector<string> tokens;
	stringstream ss(line);
	string word;

	while (ss >> word)
		tokens.push_back(word);

	return tokens;
}

string removeComment(const string& line) {
	size_t pos = line.find(';');
	if (pos != string::npos)
		return line.substr(0, pos);
	return line;
}

void write_code(babyword_t* memory, size_t size, string code) {
	auto lines = splitLines(code);
	int ip = 0;
	int dp = size - 1;
	map<string, babyword_t> labels;

	for (auto& line : lines) {
		line = removeComment(line);
		line = trim(line);
		if (line.empty()) continue;

		auto tokens = split(line);

		if (tokens.size() >= 3 && tokens[0].back() == ':') {
			string label = tokens[0].substr(0, tokens[0].size() - 1);
			labels[label] = dp;
			dp--;
		}

		else if (tokens.size() == 1 && tokens[0].back() == ':') {
			string label = tokens[0].substr(0, tokens[0].size() - 1);
			labels[label] = ip;
			cout << "(BabyCompiler) Code label: " << label << " at " << ip << "\n";
		}

		else if (tokens.size() >= 1) {
			string mnemonic = tokens[0];
			if (mnemonic != "ENT") {
				ip++;
			}
		}
	}

	ip = 0;
	dp = size - 1;

	for (auto& line : lines) {
		line = removeComment(line);
		line = trim(line);
		if (line.empty()) continue;

		auto tokens = split(line);

		cout << "(BabyCompiler) Parsing line: " << line << "\n";

		if (tokens.size() >= 3 && tokens[0].back() == ':') {
			string label = tokens[0].substr(0, tokens[0].size() - 1);
			string directive = tokens[1];
			string value = tokens[2];

			memory[dp--] = stoi(value);

			cout << "(BabyCompiler) Variable: " << label << " | "
				<< directive << " | " << value << " at " << (int)labels[label] << "\n";
		}

		else if (tokens.size() == 1 && tokens[0].back() == ':') {
			continue;
		}

		else if (tokens.size() >= 1) {
			string mnemonic = tokens[0];

			if (mnemonic == "ENT")
				continue;

			auto it = opcodes.find(mnemonic);
			if (it == opcodes.end()) {
				cerr << "(BabyCompiler) Unknown instruction: " << mnemonic << endl;
				exit(1);
			}

			babyword_t opcode = it->second;
			babyword_t instr = opcode << 8;

			if (tokens.size() == 2) {
				string operand = tokens[1];

				if (operand.front() == '(' && operand.back() == ')')
					operand = operand.substr(1, operand.size() - 2);

				if (labels.find(operand) == labels.end()) {
					cerr << "(BabyCompiler) Unknown label: " << operand << endl;
					exit(1);
				}

				babyword_t addr = labels[operand];
				instr |= addr;
			}

			memory[ip++] = instr;

			cout << "(BabyCompiler) Command: " << mnemonic
				<< " with arg: " << (tokens.size() == 2 ? tokens[1] : "")
				<< " -> 0x" << hex << instr << dec << "\n";
		}
	}
}

long long eval(babyword_t* memory, size_t size, bool debug) {
	int ip = 0;
	long long instr_count = 0;
	babyword_t accumulator = 0;

	while (ip < size) {
		babyword_t instr = memory[ip];
		if (instr == 0) break;

		babyword_t opcode = instr >> 8;
		babyword_t addr = instr & 0xFF;

		if (debug) {
			cout << "BABY AT " << ip << " SEE 0x" << hex << (int)opcode
				<< "(0x" << setfill('0') << setw(2) << hex << (int)addr << ")";
		}

		bool halt = false;
		bool jumped = false;

		switch (opcode) {
		case 0x0: // LDA
			if (addr >= size) {
				if (debug) cout << " AND CRY `out of memory`\n";
			}
			else {
				accumulator = memory[addr];
				if (debug) cout << " AND SAY `ACC now " << dec << accumulator << "`\n";
			}
			break;

		case 0x1: // STA
			if (addr >= size) {
				if (debug) cout << " AND CRY `out of memory`\n";
			}
			else {
				memory[addr] = accumulator;
				if (debug) cout << " AND SAY `mem[" << dec << (int)addr << "] now " << accumulator << "`\n";
			}
			break;

		case 0x3: // NEG
			accumulator = -accumulator;
			if (debug) cout << " AND FLIP `ACC now " << dec << (int16_t)accumulator << "`\n";
			break;

		case 0x4: // JMP
			ip = addr;
			jumped = true;
			if (debug) cout << " AND JUMP `leap to " << dec << (int)addr << "`\n";
			break;

		case 0x6: // 1DIV
			if (accumulator == 0) {
				if (debug) cout << " AND CRY `division by zero`\n";
			}
			else {
				accumulator = 1 / accumulator;
				if (debug) cout << " AND DIVIDE `1/ACC, ACC now " << dec << accumulator << "`\n";
			}
			break;

		case 0xA: // ADD
			if (addr >= size) {
				if (debug) cout << " AND CRY `out of memory`\n";
			}
			else {
				accumulator += memory[addr];
				if (debug) cout << " AND SAY `added mem[" << dec << (int)addr << "], ACC now " << accumulator << "`\n";
			}
			break;

		case 0xB: // MULT
			if (addr >= size) {
				if (debug) cout << " AND CRY `out of memory`\n";
			}
			else {
				accumulator *= memory[addr];
				if (debug) cout << " AND SAY `mult mem[" << dec << (int)addr << "], ACC now " << accumulator << "`\n";
			}
			break;

		case 0xC: // IPRT
			if (debug) cout << " AND SHOUT `";
			cout << dec << (int16_t)accumulator;
			if (debug) cout << "`\n";
			else cout << "\n";
			break;

		case 0xD: // JNP
			if ((signed)accumulator <= 0) {
				ip = addr;
				jumped = true;
				if (debug) cout << " AND SNEAK `ACC<=0, leap to " << dec << (int)addr << "`\n";
			}
			else {
				if (debug) cout << " AND STAY `ACC>0, no jump`\n";
			}
			break;

		case 0xF: // HLT
			if (debug) cout << " AND SLEEP `HALT`\n";
			halt = true;
			break;

		default:
			if (debug) cout << " AND CONFUSED `unknown opcode`\n";
			break;
		}

		if (halt) break;
		if (!jumped) ip++;
		instr_count++;
	}

	return instr_count;
}

void write_defaults(babyword_t *memory, size_t size) {
	write_code(memory, size,
		"counter: ds 5;\n"
		"one:     ds 1;\n"
		"ENT\n"
		"loop:\n"             // метка на адресе 0
		"LDA (counter);\n"    // 0
		"IPRT;\n"             // 1
		"NEG;\n"              // 2
		"ADD (one);\n"        // 3: ACC = -counter + 1
		"NEG;\n"              // 4: ACC = counter - 1
		"STA (counter);\n"    // 5
		"JNP (done);\n"       // 6: если <= 0, выходим
		"JMP (loop);\n"       // 7: иначе повторяем
		"done:\n"             // метка на адресе 8
		"HLT;\n"              // 8
	);
}

int main() {
	babyword_t memory[BABYMEM_SIZE];

	write_defaults(memory, BABYMEM_SIZE);

	cout << "\nMemory:\n\n";

	for (size_t i = 0; i < BABYMEM_SIZE; i++) {
		cout << setfill('0') << setw(3) << hex << (int)memory[i] << ' ';
	}

	cout << "\n\nRuntime (with debug message):\n\n";

	eval(memory, BABYMEM_SIZE, true);

	auto start = chrono::high_resolution_clock::now();

	write_defaults(memory, BABYMEM_SIZE);
	long long instructions = eval(memory, BABYMEM_SIZE, false);

	auto end = chrono::high_resolution_clock::now();

	auto duration = chrono::duration_cast<chrono::microseconds>(end - start);

	double seconds = duration.count() / 1000000.0;
	double ips = instructions / seconds;

	cout << "Instructions:  " << instructions << "\n";
	cout << "Time:          " << duration.count() << " us (" << seconds << " s)\n";
	cout << "Speed:         " << fixed << setprecision(0) << ips << " instructions/sec\n";

	return 0;
}
