import requests

def send_discord_notification(msg: str):
    webhook_url = "https://discord.com/api/webhooks/1355575170483360015/mqO26hwwyVvA-NOsbw0ftmlnq4FJKqKIKWjjgkK6UWgdYvN3TXJqaRqx1AZchVKEkg_g"
    data = {"content": msg}
    requests.post(webhook_url, json=data)
    

def send_discord_prime_notification(msg: str):
    webhook_url = "https://discord.com/api/webhooks/1363077971468095709/xRiS7ifA3kuBhvw6Q-2wnY-rW98u3IoNz6TupPAnLFghRlRiKZS-KuloWviIxVUTb8LG"
    data = {"content": msg}
    requests.post(webhook_url, json=data)