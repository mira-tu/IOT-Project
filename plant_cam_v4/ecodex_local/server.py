from __future__ import annotations

import argparse
import json
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

import numpy as np

from ecodex_ml import (
    extract_features_from_bytes,
    load_model,
    predict_scores,
    standardize_single,
    top_predictions,
)


DEFAULT_MODEL = Path(__file__).resolve().parent / "model" / "ecodex_baseline.npz"
DEFAULT_FACTS = Path(__file__).resolve().parent / "model" / "plant_facts.json"


def load_facts(facts_path: Path) -> dict:
    if not facts_path.exists():
        return {}
    return json.loads(facts_path.read_text(encoding="utf-8"))


def fallback_facts(label: str) -> dict:
    if label == "unknown":
        return {
            "display_name": "Unknown Plant",
            "scientific_name": "Unclassified",
            "about": "EcoDex does not have enough evidence to match this to a trained plant yet.",
            "light": "Unknown",
            "water": "Unknown",
            "fun_fact": "Adding more examples to the dataset helps EcoDex tell new plants apart.",
        }

    display_name = label.replace("_", " ").title()
    return {
        "display_name": display_name,
        "scientific_name": "Not yet added",
        "about": f"{display_name} is in the EcoDex classifier, but its full card has not been authored yet.",
        "light": "Not yet added",
        "water": "Not yet added",
        "fun_fact": "You can expand this card any time by editing plant_facts.json.",
    }


def build_prediction(
    model: dict,
    facts_by_label: dict,
    image_bytes: bytes,
    threshold: float | None = None,
) -> dict:
    labels: list[str] = model["labels"]
    feature = extract_features_from_bytes(image_bytes)
    feature = standardize_single(feature, model["feature_mean"], model["feature_std"])
    scores = predict_scores(
        feature_vector=feature,
        train_features=model["train_features"],
        train_labels=model["train_labels"],
        class_names=labels,
        k=model["k"],
    )

    best_index = int(np.argmax(scores))
    best_label = labels[best_index]
    confidence = float(scores[best_index])
    threshold = model["suggested_threshold"] if threshold is None else threshold
    accepted = confidence >= threshold
    facts = facts_by_label.get(best_label, fallback_facts(best_label))

    if not accepted:
        message = "Cannot confidently identify this plant yet."
    elif best_label == "unknown":
        message = "This looks outside the plants EcoDex currently knows."
    else:
        message = f"EcoDex thinks this is {best_label.replace('_', ' ')}."

    return {
        "accepted": accepted,
        "label": best_label,
        "display_name": facts.get("display_name", best_label.replace("_", " ").title()),
        "confidence": round(confidence, 4),
        "threshold": round(float(threshold), 4),
        "message": message,
        "facts": facts,
        "top3": top_predictions(scores, labels, limit=3),
    }


class EcoDexHandler(BaseHTTPRequestHandler):
    model: dict = {}
    facts_by_label: dict = {}

    def do_OPTIONS(self) -> None:  # noqa: N802
        self.send_response(HTTPStatus.NO_CONTENT)
        self._set_cors_headers()
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Max-Age", "86400")
        self.end_headers()

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path in {"/", "/health"}:
            self._json_response(
                {
                    "status": "ok",
                    "labels": self.model["labels"],
                    "threshold": self.model["suggested_threshold"],
                    "facts_available": sorted(self.facts_by_label.keys()),
                }
            )
            return

        self._error_response(HTTPStatus.NOT_FOUND, "Use GET /health or POST /predict")

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path != "/predict":
            self._error_response(HTTPStatus.NOT_FOUND, "Use POST /predict")
            return

        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0:
            self._error_response(HTTPStatus.BAD_REQUEST, "Request body must contain image bytes")
            return

        raw = self.rfile.read(length)
        query = parse_qs(parsed.query)
        threshold = None
        if "threshold" in query:
            try:
                threshold = float(query["threshold"][0])
            except ValueError:
                self._error_response(HTTPStatus.BAD_REQUEST, "Threshold must be a number")
                return

        try:
            result = build_prediction(self.model, self.facts_by_label, raw, threshold=threshold)
        except ValueError as exc:
            self._error_response(HTTPStatus.BAD_REQUEST, str(exc))
            return

        self._json_response(result)

    def log_message(self, format: str, *args) -> None:
        return

    def _error_response(self, status: HTTPStatus, message: str) -> None:
        body = json.dumps({"error": message}).encode("utf-8")
        self.send_response(status)
        self._set_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _json_response(self, payload: dict) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(HTTPStatus.OK)
        self._set_cors_headers()
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _set_cors_headers(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the local EcoDex prediction server.")
    parser.add_argument("--model", type=Path, default=DEFAULT_MODEL, help="Path to the trained .npz model.")
    parser.add_argument("--facts", type=Path, default=DEFAULT_FACTS, help="Path to plant facts JSON.")
    parser.add_argument("--host", default="0.0.0.0", help="Host address to bind.")
    parser.add_argument("--port", type=int, default=8090, help="Port for the local API.")
    args = parser.parse_args()

    model = load_model(args.model)
    facts_by_label = load_facts(args.facts)
    EcoDexHandler.model = model
    EcoDexHandler.facts_by_label = facts_by_label
    server = ThreadingHTTPServer((args.host, args.port), EcoDexHandler)
    print(f"EcoDex server running on http://{args.host}:{args.port}")
    print("Endpoints: GET /health, POST /predict")
    server.serve_forever()


if __name__ == "__main__":
    main()
