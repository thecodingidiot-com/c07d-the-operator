# c07d-the-operator

Companion repository for **c07d — The Operator** at
[thecodingidiot.com](https://thecodingidiot.com), the final part of the
c07 arc (c07a → c07b → c07c → c07d).

---

## Follow my journey

Working through c07d alongside the implementation pages? Continue from
your own `c07c-practice` directory, or start here:

```bash
git clone https://github.com/thecodingidiot-com/c07d-the-operator.git c07d-practice
cd c07d-practice/solution
make -C libtci re
make re
bash ../test.sh
```

All tests must pass before the chapter is complete.

---

## Follow your journey

Building `c07shell` independently? Here is the full project brief for
this part of the arc (assumes c07a/b/c are already done):

- The shell owns its own environment — a heap-allocated copy of what it
  inherited, never a bare pointer into `environ`. `export NAME=VALUE`,
  `export NAME`, `unset NAME`, and `env` all operate on this owned copy.
- `$VAR` and `$?` expand to their values in every command's arguments,
  once per command, before execution.
- The shell survives its own Ctrl+C (`SIGINT`) — only the foreground
  child dies, restored to default disposition right before it execs.
  `SIGQUIT` is ignored at the shell prompt, matching bash.
- Wildcards (`*`, `?`, `[...]`) expand against real filenames via
  `glob.h`, after variable expansion.
- `&&`, `||`, and `( ... )` subshells execute for real — short-circuit
  evaluation, and a subshell's own `cd`/`export`/etc. never affect the
  parent shell.

**Tester:** `test.sh` is a bash-comparison suite — every case (including
every case from c07a/b/c) is piped as one script to both `c07shell` and
to `bash`; stdout, stderr, and exit status are diffed. Bash is the
oracle for the whole arc.

---

## License

MIT — see [LICENSE](LICENSE).
