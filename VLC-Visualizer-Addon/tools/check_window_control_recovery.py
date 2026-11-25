"""Exercise the production window registry with isolated child processes."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile


def check(probe, root):
    env = dict(os.environ, LOCALAPPDATA=str(root), HOME=str(root), XDG_RUNTIME_DIR=str(root))
    directory = {
        "win32": root / "VLCVisualizers/control",
        "darwin": root / "Library/Application Support/VLCVisualizers/control",
    }.get(sys.platform, root / "vlc-visualizers/control")
    children = []

    def register(suite="suite-a", visualizer="terrain", owner="42"):
        child = subprocess.Popen(
            [probe, "register", suite, visualizer, owner], cwd=root, env=env, text=True,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
        )
        children.append(child)
        assert child.stdout.readline().strip() == "ready"
        paths = list(directory.glob(f"{child.pid}-*.json"))
        assert len(paths) == 1
        return child, paths[0]

    def request(action, suite="suite-a", visualizer="terrain", owner="42"):
        return int(subprocess.check_output(
            [probe, action, suite, visualizer, owner], cwd=root, env=env, timeout=5, text=True,
        ))

    def response(child, message):
        child.stdin.write(message + "\n")
        child.stdin.flush()
        return child.stdout.readline().strip()

    def sidecar(record, action):
        return record.with_name(record.name + "." + action)

    def discard(child):
        child.kill()
        child.wait(timeout=5)

    try:
        old, stale = register()
        assert request("input") == request("close") == 1
        discard(old)
        assert stale.is_file() and all(sidecar(stale, key).is_file() for key in ("input", "close"))

        current, active = register()
        other, unrelated = register("suite-b")
        _, other_owner = register(owner="43")
        _, other_id = register(visualizer="geiss")
        assert request("close") == 1, "dead registration counted as a live window"
        assert not any(path.exists() for path in (stale, sidecar(stale, "input"), sidecar(stale, "close")))
        assert response(current, "close") == "1"
        assert response(other, "close") == "0"
        assert unrelated.is_file() and other_owner.is_file() and other_id.is_file()
        assert not sidecar(other_owner, "close").exists() and not sidecar(other_id, "close").exists()

        metadata = json.loads(active.read_text(encoding="utf-8"))
        assert metadata.get("process_start"), "process creation identity unavailable"
        previous = dict(metadata, process_start=metadata["process_start"] + "-old", token="0" * 32)
        reused = directory / f"{current.pid}-{previous['token']}.json"
        reused.write_text(json.dumps(previous), encoding="utf-8")
        assert request("input") == 1, "registration from an earlier process counted after PID reuse"
        assert response(current, "input") == "1" and response(current, "input") == "0"
        assert not reused.exists()

        # A previous release registered only a PID. It must still accept
        # Close and input requests while alive and be pruned on exit.
        metadata.pop("process_start")
        active.write_text(json.dumps(metadata), encoding="utf-8")
        assert request("input") == 1 and response(current, "input") == "1"
        assert request("close") == 1 and response(current, "close") == "1"
        discard(current)
        assert request("input") == 0 and not active.exists()

        print("Window control recovery passed: forced exit, stale sidecars, PID reuse, live legacy and isolation")
    finally:
        for child in children:
            if child.poll() is None:
                child.stdin.write("quit\n")
                child.stdin.flush()
                try:
                    child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    discard(child)
            child.stdin.close()
            child.stdout.close()


if __name__ == "__main__":
    executable = str(Path(sys.argv[1]).resolve(strict=True))
    with tempfile.TemporaryDirectory(prefix="visualizer-control-") as temporary:
        check(executable, Path(temporary))
