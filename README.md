# 🖥️ Smart Process Monitor (`procmon`)

A lightweight terminal-based system monitoring tool written in C, leveraging the `ncurses` library.  
It displays system uptime, load average, memory usage, CPU information, and the top 5 CPU-consuming processes dynamically in a colorful and interactive terminal UI.

---

## 🚀 Features

Display real-time system statistics:

- System uptime
- Load average
- Memory usage (total, used, free)
- CPU model, cores, and frequency
- Top 5 processes by CPU usage, updated live

Interactive controls:

- Press `q` to quit the program
- Press `p` to pause or resume the monitoring
- Press `+` or `-` to increase or decrease the refresh interval (1 to 10 seconds)

✨ Uses `ncurses` for a clean and colorful terminal interface

---

## 🗂️ Project Structure

```
procmon/
├── main.c         # Source code of the process monitor  
├── procmon        # Compiled executable (ignored in .gitignore)  
├── README.md      # This project documentation  
└── .gitignore     # Git ignore rules  
```

---

## 🛠️ How to Compile

Make sure you have GCC and the ncurses library installed. Compile the program with:

```bash
gcc main.c -o procmon -lncurses
```

---

## ▶️ How to Run

Run the compiled executable in your terminal:

```bash
./procmon
```

You will see a dashboard showing system stats and the top CPU-consuming processes updating every few seconds.

---

## 🎮 Usage Controls

- `q` : Quit the monitor  
- `p` : Pause or resume updating the data  
- `+` : Increase refresh interval by 1 second (max 10 seconds)  
- `-` : Decrease refresh interval by 1 second (min 1 second)

---

## 📦 Requirements

- GCC compiler  
- ncurses library (`sudo apt install libncurses5-dev` on Debian/Ubuntu)  
- Unix/Linux environment (tested on Ubuntu and Kali Linux)

---

## 👨‍💻 Author

**Vigneshwaran Murugan**  
[🔗 GitHub Profile](https://github.com/VigneshwaranMurugan16/)

---

## 📝 License

This project is open-source and licensed under the MIT License.
