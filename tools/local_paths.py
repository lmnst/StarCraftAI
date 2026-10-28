"""Machine paths shared by the Windows experiment tools."""
import json
import os
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


def load_paths():
    config = Path(os.environ.get("STARCRAFTAI_PATHS", REPO / "local-paths.json"))
    if not config.is_file():
        raise FileNotFoundError(
            f"Copy local-paths.example.json to {config} and set your dependency paths"
        )
    values = json.loads(config.read_text(encoding="utf-8-sig"))
    return {name: (REPO / value).resolve() for name, value in values.items()}
