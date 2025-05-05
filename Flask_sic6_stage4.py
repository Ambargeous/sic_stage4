from flask import Flask, request, jsonify
from pymongo import MongoClient
from datetime import datetime
import pytz
import google.generativeai as genai

app = Flask(__name__)

# Koneksi ke MongoDB
client = MongoClient("mongodb+srv://abrisamgrup:man12345@cluster0.vddzxpx.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0")
db = client["health_monitoring"]
collection = db["sensor_data"]
gemini_collection = db["gemini_responses"]  # Koleksi baru untuk simpan jawaban Gemini

# Konfigurasi Gemini
genai.configure(api_key="AIzaSyAqOlUTj_MyNM0kL48zhaPNzsmd4UK_ZB4")  # Ganti dengan API key kamu
model = genai.GenerativeModel("gemini-2.0-flash")


@app.route("/data", methods=["POST"])
def receive_data():
    data = request.json
    heart_rate = data.get("heart_rate")
    temperature = data.get("temperature")
    siswa = data.get("siswa")  # Ambil nilai siswa dari ESP32

    if heart_rate is None or temperature is None:
        return jsonify({"status": "error", "message": "Invalid data"}), 400

    local_tz = pytz.timezone('Asia/Jakarta')
    timestamp_local = datetime.now(local_tz)

    document = {
        "heart_rate": heart_rate,
        "temperature": temperature,
        "timestamp": timestamp_local,
        "siswa": siswa  # Simpan nilai siswa ke MongoDB
    }

    collection.insert_one(document)
    return jsonify({"status": "success", "message": "Data saved"}), 200


@app.route("/ask_gemini", methods=["POST"])
def ask_gemini():
    data = request.get_json()
    question = data.get("question")

    if not question:
        return jsonify({"error": "Pertanyaan tidak ditemukan"}), 400

    try:
        response = model.generate_content(question)
        answer = response.text

        # Simpan ke MongoDB
        timestamp_local = datetime.now(pytz.timezone("Asia/Jakarta"))
        gemini_document = {
            "question": question,
            "answer": answer,
            "timestamp": timestamp_local
        }
        gemini_collection.insert_one(gemini_document)

        return jsonify({"response": answer}), 200
    except Exception as e:
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
