<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'

const metrics = ref({
    status: 'disconnected',
    temp: 0,
    desc: '',
    windspeed: 0,
    humidity: 0,
    place: 'Haugesund'
})

onMounted(() => {
    const es = new EventSource('http://localhost:8080/metrics/stream')
    es.onmessage = (ev) => {
        const raw = JSON.parse(ev.data) as any
        let content: Record<string, any> = {}

        try {
            content = JSON.parse(raw.content)
        } catch {
            content = { desc: raw.content }
        }

        metrics.value = {
            status: raw.status ?? 'disconnected',
            temp: content.temp ?? 0,
            desc: content.desc ?? String(raw.content),
            windspeed: content.windspeed ?? 0,
            humidity: content.humidity ?? 0,
            place: content.place ?? 'Haugesund'
        }
    }
    es.onerror = () => {
        metrics.value.status = 'disconnected'
        es.close()
    }
    onUnmounted(() => es.close())
})
</script>

<template>
    <div class="app">
        <h1>📍 {{ metrics.place }}</h1>
        <p class="status">{{ metrics.status === 'connected' ? '● LIVE' : '● FRAKOBLET' }}</p>
        <p class="temp">{{ metrics.temp }}°C</p>
        <p class="desc">{{ metrics.desc }}</p>
        <p>💨 {{ metrics.windspeed }} m/s</p>
        <p>💧 {{ metrics.humidity }}%</p>
    </div>
</template>

<style scoped>
.app {
    min-height: 100vh;
    background: linear-gradient(135deg, #1a1a2e, #0f3460);
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    color: white;
    font-family: sans-serif;
    text-align: center;
}

h1 { font-size: 1.5rem; margin-bottom: 0.5rem; }
.status { color: #28a745; margin-bottom: 1rem; }
.temp { font-size: 5rem; font-weight: bold; margin: 0; }
.desc { font-size: 1.3rem; opacity: 0.8; margin-bottom: 1rem; text-transform: capitalize; }
p { font-size: 1.1rem; margin: 0.3rem; }
</style>