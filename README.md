# Monitor Hídrico 3D

Representación 3D interactiva del proyecto IoT de agua para la granja: pozo, tanque de reserva, casa con tinaco y tres bebederos (vacas, caballos y cerdos), controlados por un ESP32 con sensores HC-SR04, mini bombas y módulos de relé.

## Cómo abrirlo

Abre `index.html` (simulación de la granja) o `maqueta.html` (maqueta física) en Chrome, Edge o Firefox. Necesita internet porque carga Three.js desde un CDN. Cada página tiene un botón para pasar a la otra.

## Guía de conexión (`guia-conexion.html`) y código (`firmware/`)

Paso a paso para conectar las piezas reales (ESP32, relé de 2 canales, MB102, mini bombas, HC-SR04), con los problemas de voltaje resueltos: los relés del módulo son de 24 V (`SRD-24VDC`), el ESP32 trabaja a 3.3 V y el MB102 no aguanta más de 2 bombas a la vez. Versión básica: pozo → tanque central → bebedero 1 y bebedero 2, con 3 bombas, 3 sensores (el pozo no lleva) y el módulo de 4 relés HW-316 de 5 V (se activa con LOW). El programa está en `firmware/monitor_hidrico/monitor_hidrico.ino`.

## Conexión de prueba (`conexion-3d.html`)

Vista 3D de la prueba en la mesa: protoboard con MB102, ESP32, relé HW-316, 2 sensores y 2 bombas en vasos. Al tocar una pieza se iluminan sus cables y aparece a dónde va cada uno. La pestaña "Lista para conectar" sirve para ir marcando los cables en orden. El programa de esta prueba está en `micropython/prueba_2_bebederos.py`.

## Maqueta (`maqueta.html`)

La maqueta real de la versión básica a escala (1 unidad = 1 cm), hecha a partir de la foto de su base: plancha de hielo seco de ≈ 65 × 80 cm sobre 4 soportes y la lámina café, pozo (botella hundida), tanque central (galón cortado) y 2 bebederos (botellas acostadas, la manguera entra por la tapa) en corrales con cerca roja. Muestra dónde va el ESP32, el relé, el MB102, los sensores, las bombas, las mangueras y los cables por debajo.

- **Armado**: 11 pasos en orden, empezando por probar la electrónica en la mesa. Cada paso mueve la cámara e ilumina las piezas que usa.
- **Pieza**: toca cualquier pieza para ver material, medidas, cómo hacerla y cómo se conecta.
- **Materiales**: lista de compras y el largo de manguera y cable calculado desde el modelo.
- **Cables**: cada tramo de cable con sus pines del ESP32 y su largo.
- Botones **Por debajo** y **Rayos X** para ver el cableado, y el agua se mueve a la velocidad real de las mini bombas.

## Qué se puede hacer

- **App**: lo que vería la aplicación móvil. Muestra el nivel de cada depósito (bajo, medio o alto), la distancia que mide cada HC-SR04, el estado de las bombas, la conexión WiFi y el último paquete JSON que envió el ESP32.
- **Control**: botones para llenar, detener o consumir agua en cada depósito. La bomba se apaga sola al llegar al nivel de paro, también en modo manual. Si el depósito ya está lleno, la bomba no se enciende.
- **Fallas**: romper tuberías, desconectar sensores, perder el WiFi o simular una sequía. Si una bomba lleva varios minutos encendida y el nivel no sube, el ESP32 la apaga, la bloquea y avisa. Si fallan varios bebederos a la vez, señala la tubería principal.
- **Alertas**: registro del asistente. Con el botón "Voz" el asistente lee en voz alta las alertas.
- **Piezas**: para qué sirve cada pieza comprada, pines sugeridos del ESP32 y notas para armar la maqueta.

Para ver la escena completa usa el botón "Ocultar paneles" (o la tecla H). La flecha al final de las pestañas minimiza o abre el panel, y el botón − minimiza el asistente.

Haz clic en cualquier depósito, bomba o animal de la escena para acercar la cámara. La velocidad de simulación (1× a 300×) está en la barra superior.
