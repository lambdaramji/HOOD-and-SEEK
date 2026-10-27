"""contracts/ 검증 — CI(system.yml contracts job)와 로컬이 이 파일 하나를 똑같이 실행한다.

    python contracts/validate.py        # jsonschema==4.23.0 (CI와 같은 버전)

1. 모든 *.schema.json 의 문법 검사
2. examples/<name>*.json 은 스키마를 통과해야 하고, 한 줄로 직렬화해 4096 byte 안에 들어가야 한다 (설계서 §2.1)
3. examples/invalid/<name>.<case>.json 은 스키마에서 실패해야 한다 — 통과하면 스키마가 그 입력을 못 막는다는 뜻
"""

import json
import pathlib
import sys

from jsonschema import Draft202012Validator, validators
from jsonschema.exceptions import best_match

ROOT = pathlib.Path(__file__).resolve().parent
MAX_LINE_BYTES = 4096


def load(path):
    return json.loads(path.read_text(encoding="utf-8"))


def rel(path):
    return path.relative_to(ROOT.parent)


def main():
    failed = 0
    checkers = {}
    for schema_path in sorted(ROOT.glob("*.schema.json")):
        schema = load(schema_path)
        cls = validators.validator_for(schema, default=Draft202012Validator)
        cls.check_schema(schema)
        name = schema_path.name.removesuffix(".schema.json")
        checkers[name] = cls(schema)
        examples = sorted((ROOT / "examples").glob(f"{name}*.json"))
        if not examples:
            print(f"!! {rel(schema_path)}: 예제 없음 (contracts/examples/{name}*.json)")
            failed += 1
            continue
        for ex in examples:
            doc = load(ex)
            errs = sorted(checkers[name].iter_errors(doc), key=lambda e: list(e.path))
            for e in errs:
                print(f"!! {rel(ex)}: {list(e.path)}: {e.message}")
            line = json.dumps(doc, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
            if len(line) + 1 > MAX_LINE_BYTES:
                print(f"!! {rel(ex)}: 한 줄 {len(line) + 1} byte > {MAX_LINE_BYTES}")
                errs.append(None)
            failed += len(errs)
            print(("ok " if not errs else "FAIL ") + str(rel(ex)))

    for ex in sorted((ROOT / "examples" / "invalid").glob("*.json")):
        name = ex.name.split(".", 1)[0]
        if name not in checkers:
            print(f"!! {rel(ex)}: 짝 스키마 없음 (contracts/{name}.schema.json)")
            failed += 1
            continue
        err = best_match(checkers[name].iter_errors(load(ex)))
        if err is None:
            print(f"FAIL {rel(ex)}: 실패해야 할 입력이 통과함")
            failed += 1
        else:
            print(f"ok (거부) {rel(ex)}: {list(err.path)}: {err.message}")

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
