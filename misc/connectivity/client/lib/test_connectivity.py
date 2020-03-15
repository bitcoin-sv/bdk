#!/usr/bin/env python3
import os
import requests
from requests.auth import HTTPBasicAuth

os.environ['NO_PROXY'] = '127.0.0.1'

response = requests.get('http://127.0.0.1:8080')
#response = requests.get('https://127.0.0.1:443', verify='self_signed_certificate.pem')
reply_str = response.content.decode(response.encoding)
print(reply_str)