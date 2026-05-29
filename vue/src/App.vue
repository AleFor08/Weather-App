<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue'

const metrics = ref({
    status: 'disconnected',
    updateFrequency: 0,
    timePerUpdate: 0,
    content: ''
})

const error = ref('')

onMounted(() => {
    const es = new EventSource('http://localhost:8080/metrics/stream');
    es.onmessage = (ev) => {
        try {
            metrics.value = JSON.parse(ev.data);
            error.value = '';
        } catch (e) {
            error.value = 'Error: ' + e;
        }
    };
    es.onerror = (err) => {
        error.value = 'SSE-error: ' + err;
        metrics.value.status = 'disconnected';
        es.close();
    };
    onUnmounted(() => es.close());
});

const isActive = computed(() => {
    const s = String(metrics.value.status ?? '').toLowerCase()
    return s === 'connected' || s === 'online' || s === 'up' || s === 'true'
})

const weatherEmoji = computed(() => {
    const content = (metrics.value.content || '').toLowerCase()
    if (content.includes('sol') || content.includes('clear')) return '☀️'
    if (content.includes('sky') || content.includes('cloud')) return '⛅'
    if (content.includes('regn') || content.includes('rain')) return '🌧️'
    if (content.includes('snø') || content.includes('snow')) return '❄️'
    return '🌤️'
})
</script>

<template>
    <div class="weather-app">
        <div class="weather-card">
            <div class="location">
                <span>📍 Oslo, Norge</span>
                <span class="status-badge" :class="isActive ? 'online' : 'offline'">
                    {{ isActive ? '● LIVE' : '● FRAKOBLET' }}
                </span>
            </div>

            <div class="weather-main">
                <div class="emoji">{{ weatherEmoji }}</div>
                <div class="description">{{ metrics.content || 'Henter data...' }}</div>
            </div>

            <div class="weather-stats">
                <div class="stat">
                    <span class="stat-label">Oppdatering</span>
                    <span class="stat-value">{{ metrics.updateFrequency }} ms</span>
                </div>
                <div class="stat">
                    <span class="stat-label">Tid per oppdatering</span>
                    <span class="stat-value">{{ metrics.timePerUpdate }} ns</span>
                </div>
            </div>

            <div v-if="error" class="error">{{ error }}</div>
        </div>
    </div>
</template>

<style scoped>
.weather-app {
    min-height: 100vh;
    background: linear-gradient(135deg, #1a1a2e, #16213e, #0f3460);
    display: flex;
    align-items: center;
    justify-content: center;
    font-family: 'Segoe UI', sans-serif;
}

.weather-card {
    background: rgba(255, 255, 255, 0.1);
    backdrop-filter: blur(10px);
    border-radius: 24px;
    padding: 2.5rem;
    width: 360px;
    color: white;
    border: 1px solid rgba(255, 255, 255, 0.2);
}

.location {
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 14px;
    opacity: 0.8;
    margin-bottom: 2rem;
}

.status-badge.online { color: #28a745; }
.status-badge.offline { color: #dc3545; }

.weather-main {
    text-align: center;
    margin-bottom: 2rem;
}

.emoji {
    font-size: 80px;
    margin-bottom: 0.5rem;
}

.description {
    font-size: 22px;
    text-transform: capitalize;
    opacity: 0.9;
}

.weather-stats {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 1rem;
}

.stat {
    background: rgba(255, 255, 255, 0.1);
    border-radius: 12px;
    padding: 1rem;
    text-align: center;
}

.stat-label {
    display: block;
    font-size: 11px;
    opacity: 0.6;
    text-transform: uppercase;
    letter-spacing: 1px;
    margin-bottom: 0.4rem;
}

.stat-value {
    font-size: 20px;
    font-weight: 600;
}

.error {
    margin-top: 1rem;
    color: #ff6b6b;
    font-size: 13px;
    text-align: center;
}
</style>