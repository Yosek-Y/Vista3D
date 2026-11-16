# Monitor Hídrico 3D

Representación 3D interactiva del proyecto IoT de agua para la granja: pozo, tanque de reserva, casa con tinaco y tres bebederos (vacas, caballos y cerdos), controlados por un ESP32 con sensores HC-SR04, mini bombas y módulos de relé.

## Cómo abrirlo

Abre `index.html` en Chrome, Edge o Firefox. Necesita internet la primera vez porque carga Three.js desde un CDN.

## Qué se puede hacer

- **App**: lo que vería la aplicación móvil. Muestra el nivel de cada depósito (bajo, medio o alto), la distancia que mide cada HC-SR04, el estado de las bombas, la conexión WiFi y el último paquete JSON que envió el ESP32.
- **Control**: botones para llenar, detener o consumir agua en cada depósito. La bomba se apaga sola al llegar al nivel de paro, también en modo manual. Si el depósito ya está lleno, la bomba no se enciende.
- **Fallas**: romper tuberías, desconectar sensores, perder el WiFi o simular una sequía. Si una bomba lleva varios minutos encendida y el nivel no sube, el ESP32 la apaga, la bloquea y avisa. Si fallan varios bebederos a la vez, señala la tubería principal.
- **Alertas**: registro del asistente. Con el botón "Voz" el asistente lee en voz alta las alertas.
- **Piezas**: para qué sirve cada pieza comprada, pines sugeridos del ESP32 y notas para armar la maqueta.

Haz clic en cualquier depósito, bomba o animal de la escena para acercar la cámara. La velocidad de simulación (1× a 300×) está en la barra superior.
