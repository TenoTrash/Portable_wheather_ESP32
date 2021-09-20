# Portable_wheather_ESP32
Monitoreo de clima portatil con ESP32 y BME280

Con una pantalla OLED y un par de pantallas con:

- Resumen de clima
- Temperatura
- Humedad
- Presion
- Altitud (relativa Buenos Aires con respecto a la presón atmosférica)
- Creditos

Se van ciclando las pantallas con un pulsador (con control anti rebote simple)

Las lecturas del BME280 se hacen cada minuto, en el menor sampleo posible, según lo indicado por el fabricante en la hoja de datos. Esto minimiza el diferencial de temperatura debido al consumo de energía y mejora notablemente la presición de la medición.
