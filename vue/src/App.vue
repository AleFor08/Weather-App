<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'

const weather = ref({
  status: 'disconnected',
  temp: null,
  feels_like: null,
  description: '',
  humidity: null,
  wind_speed: null,
  temp_min: null,
  temp_max: null,
})

onMounted(() => {
  const es = new EventSource('http://localhost:8080/metrics/stream')
  es.onmessage = (ev) => {
    weather.value = { ...JSON.parse(ev.data), status: 'connected' }
  }
  es.onerror = () => {
    weather.value.status = 'disconnected'
    es.close()
  }
  onUnmounted(() => es.close())
})
</script>

<template>
  <div class="wx">
    <div class="wx-card">
      <div class="wx-city">📍 Oslo, Norge</div>
      <div class="wx-temp">{{ weather.temp ?? '--' }}°</div>
      <div class="wx-desc">{{ weather.description || 'Henter data...' }}</div>
      <div class="wx-grid">
        <div class="wx-stat">
          <div class="wx-stat-label">Føles som</div>
          <div class="wx-stat-value">{{ weather.feels_like ?? '--' }}°C</div>
        </div>
        <div class="wx-stat">
          <div class="wx-stat-label">Luftfu