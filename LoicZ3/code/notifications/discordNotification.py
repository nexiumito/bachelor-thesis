import requests

def send_discord_notification(msg: str):
    webhook_url = ""
    data = {"content": msg}
    requests.post(webhook_url, json=data)
    

def send_discord_prime_notification(msg: str):
    webhook_url = ""
    data = {"content": msg}
    requests.post(webhook_url, json=data)