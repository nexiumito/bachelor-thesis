"""notifier.py — notifications externes (Discord webhook ou Telegram bot).

Lit les credentials EXCLUSIVEMENT dans l'environnement :
  - channel='discord'  -> DISCORD_WEBHOOK_URL
  - channel='telegram' -> TELEGRAM_TOKEN, TELEGRAM_CHAT_ID

Si la variable attendue est absente -> mode no-op silencieux. Le bench tourne
normalement, juste sans notification. Idem si ``requests`` indisponible ou si
le reseau echoue.

JAMAIS de webhook/token committe dans le repo : tout passe par les variables
d'environnement.
"""

from __future__ import annotations

import logging
import os
import threading
import time
from typing import Any

logger = logging.getLogger(__name__)

# Import optionnel de requests : si absent, le notifier passe en no-op.
try:
    import requests  # type: ignore
    _REQUESTS_AVAILABLE = True
except ImportError:
    _REQUESTS_AVAILABLE = False


# ---------------------------------------------------------------------------
# Backends.
# ---------------------------------------------------------------------------
class _DiscordBackend:
    """Webhook Discord, message simple. Cap a 2000 caracteres (limite Discord).

    Sur HTTP 429 (rate limit), un re-essai avec ``Retry-After`` est tente une
    seule fois. Au-dela, on abandonne et on logge un warning rate-limite.
    """

    def __init__(self, webhook_url: str, timeout_s: int = 10) -> None:
        self.url = webhook_url
        self.timeout = timeout_s
        self._last_failure_logged = 0.0  # rate-limit des warnings

    def send(self, content: str, embed: dict[str, Any] | None = None) -> bool:
        if not _REQUESTS_AVAILABLE:
            return False
        payload: dict[str, Any] = {"content": content[:2000]}
        if embed:
            payload["embeds"] = [embed]
        try:
            r = requests.post(self.url, json=payload, timeout=self.timeout)
            if r.status_code == 429:
                # Backoff sur rate-limit. Discord renvoie Retry-After (s).
                retry_after = float(r.headers.get("Retry-After", "1"))
                # Cap a 10s pour ne pas bloquer le thread heartbeat.
                wait = min(retry_after, 10.0)
                logger.info("Notifier Discord rate-limit, retry dans %.1fs", wait)
                time.sleep(wait)
                r = requests.post(self.url, json=payload, timeout=self.timeout)
            r.raise_for_status()
            return True
        except Exception as exc:
            self._maybe_log_failure(exc)
            return False

    def _maybe_log_failure(self, exc: Exception) -> None:
        now = time.time()
        if now - self._last_failure_logged > 600:
            logger.warning("Notifier Discord echec : %s", exc)
            self._last_failure_logged = now


class _TelegramBackend:
    """Bot Telegram via API HTTP brute (pas de SDK)."""

    def __init__(self, token: str, chat_id: str, timeout_s: int = 10) -> None:
        self.url = f"https://api.telegram.org/bot{token}/sendMessage"
        self.chat_id = chat_id
        self.timeout = timeout_s
        self._last_failure_logged = 0.0

    def send(self, content: str, embed: dict[str, Any] | None = None) -> bool:
        # ``embed`` ignore : pas de notion d'embed cote Telegram, le markdown
        # de fallback fait le travail.
        del embed
        if not _REQUESTS_AVAILABLE:
            return False
        payload = {
            "chat_id": self.chat_id,
            "text": content[:4000],  # Telegram cap 4096
            "parse_mode": "Markdown",
        }
        try:
            r = requests.post(self.url, json=payload, timeout=self.timeout)
            r.raise_for_status()
            return True
        except Exception as exc:
            now = time.time()
            if now - self._last_failure_logged > 600:
                logger.warning("Notifier Telegram echec : %s", exc)
                self._last_failure_logged = now
            return False


# ---------------------------------------------------------------------------
# Facade publique.
# ---------------------------------------------------------------------------
class Notifier:
    """Wrapper unifie pour Discord webhook et Telegram bot.

    Mode no-op silencieux si :
      - ``enabled`` est False ;
      - ``channel`` vaut "none" ;
      - les credentials env attendus sont absents ;
      - ``requests`` n'est pas installe.

    B7 : les FAIL hard sont accumules dans un buffer interne et flushes par
    un thread daemon toutes les ``fail_hard_flush_s`` secondes (5s par
    defaut). Cela evite le rate-limit Discord (5 req / 5s) lors de rafales
    de FAIL hard et regroupe les alertes en un seul message lisible.
    """

    def __init__(self, config: dict[str, Any]) -> None:
        self.enabled = bool(config.get("enabled", False))
        self.channel = config.get("channel", "none")
        self.notify_on_start = bool(config.get("notify_on_start", True))
        self.notify_on_hard_fail = bool(config.get("notify_on_hard_fail", True))
        self.notify_on_completion = bool(config.get("notify_on_completion", True))
        self.heartbeat_interval_min = int(config.get("heartbeat_interval_min", 30))
        self.fail_hard_flush_s = float(config.get("fail_hard_flush_s", 5.0))
        self.backend = self._make_backend()

        # Buffer + thread de flush pour les FAIL hard. Le thread n'est demarre
        # que si le notifier est actif (sinon on le laisse no-op).
        self._fail_hard_buffer: list[str] = []
        self._buffer_lock = threading.Lock()
        self._stop_event = threading.Event()
        self._flush_thread: threading.Thread | None = None
        if self.is_active and self.notify_on_hard_fail:
            self._flush_thread = threading.Thread(
                target=self._flush_loop, daemon=True,
                name="NotifierFailHardFlush",
            )
            self._flush_thread.start()

    def _make_backend(self) -> Any:
        if not self.enabled or self.channel == "none":
            return None
        if not _REQUESTS_AVAILABLE:
            logger.warning("Notifier: 'requests' manquant, mode no-op")
            return None
        if self.channel == "discord":
            url = os.environ.get("DISCORD_WEBHOOK_URL")
            if not url:
                logger.info("DISCORD_WEBHOOK_URL non defini, notifier en no-op")
                return None
            return _DiscordBackend(url)
        if self.channel == "telegram":
            token = os.environ.get("TELEGRAM_TOKEN")
            chat = os.environ.get("TELEGRAM_CHAT_ID")
            if not token or not chat:
                logger.info("TELEGRAM_TOKEN / TELEGRAM_CHAT_ID non definis, notifier en no-op")
                return None
            return _TelegramBackend(token, chat)
        logger.warning("Notifier: channel inconnu '%s', mode no-op", self.channel)
        return None

    @property
    def is_active(self) -> bool:
        return self.backend is not None

    # -- API publique ---------------------------------------------------------

    def _send(self, msg: str, embed: dict[str, Any] | None = None) -> None:
        if self.backend is None:
            return
        try:
            self.backend.send(msg, embed=embed)
        except Exception as exc:
            # Un crash dans le backend ne doit jamais casser le bench.
            logger.warning("Notifier: exception ignoree : %s", exc)

    def started(self, n_tasks: int, eta_str: str) -> None:
        if not self.notify_on_start:
            return
        self._send(f"Bench demarre - {n_tasks} taches, ETA {eta_str}")

    def heartbeat(self, done: int, total: int, failed: int, eta_str: str) -> None:
        pct = 100.0 * done / total if total else 0.0
        self._send(
            f"Heartbeat : {done}/{total} ({pct:.1f}%), "
            f"{failed} echecs, ETA {eta_str}"
        )

    def fail_hard(self, instance_id: str, mode: str, invariant: str,
                  message: str) -> None:
        if not self.notify_on_hard_fail:
            return
        # B7 : buffer plutot qu'envoi immediat. Le thread de flush regroupe
        # les FAIL hard sur fenetre de fail_hard_flush_s pour eviter le
        # rate-limit Discord (5 req / 5s).
        line = f"`{invariant}` sur `{instance_id}` (mode `{mode}`) - {message}"
        with self._buffer_lock:
            self._fail_hard_buffer.append(line)

    def finished(self, summary_path: str, n_ok: int, n_fail: int,
                 duration_str: str) -> None:
        # Flush final synchrone pour ne pas perdre les derniers FAIL hard.
        self._flush_now()
        if not self.notify_on_completion:
            return
        self._send(
            f"Bench termine en {duration_str}. "
            f"{n_ok} OK / {n_fail} echecs. "
            f"`SUMMARY.md` : {summary_path}"
        )

    def error(self, message: str) -> None:
        self._send(f"Erreur fatale orchestrateur : {message}")

    def stop(self) -> None:
        """Arrete le thread de flush et flush une derniere fois."""
        self._stop_event.set()
        if self._flush_thread is not None:
            self._flush_thread.join(timeout=2.0)
        self._flush_now()

    # -- Internals -----------------------------------------------------------

    def _flush_loop(self) -> None:
        """Thread daemon : flush le buffer FAIL hard toutes les
        ``fail_hard_flush_s`` secondes. Sortie propre via _stop_event."""
        while not self._stop_event.is_set():
            self._stop_event.wait(self.fail_hard_flush_s)
            self._flush_now()

    def _flush_now(self) -> None:
        """Flush synchrone le buffer FAIL hard (no-op si vide)."""
        with self._buffer_lock:
            if not self._fail_hard_buffer:
                return
            batch = self._fail_hard_buffer
            self._fail_hard_buffer = []
        # Discord cap = 2000 chars. On formate une entete + lignes ; si trop
        # long, on tronque au-dela en mentionnant le compte total.
        header = f"FAIL hard ({len(batch)}) :\n"
        body = "\n".join(batch)
        msg = header + body
        if len(msg) > 1900:  # marge sous le cap 2000
            msg = msg[:1900] + f"\n... ({len(batch)} FAIL hard au total)"
        self._send(msg)


__all__ = ["Notifier"]
