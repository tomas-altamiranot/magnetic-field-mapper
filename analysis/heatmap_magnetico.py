import serial
import time
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.tri as tri
import csv

# --- CONFIGURACIÓN DE COMUNICACIÓN ---
PUERTO_SERIAL = 'COM6' 
BAUD_RATE = 115200

# Inicialización de vectores de datos
angulos = []
radios = []
campos = []

print("------------------------------------------------")
print(f"Iniciando conexión con sistema embebido en {PUERTO_SERIAL}...")
print("------------------------------------------------")

try:
    ser = serial.Serial(PUERTO_SERIAL, BAUD_RATE, timeout=1)
    time.sleep(2) # Tiempo de espera para reinicio del microcontrolador
    
    print("CONEXIÓN ESTABLECIDA.")
    print("Controles de Calibración:")
    print("  'a' -> Ajuste Fino (Dirección A)")
    print("  'd' -> Ajuste Fino (Dirección B)")
    print("  's' -> INICIAR SECUENCIA DE ESCANEO")
    print("------------------------------------------------")

    # --- FASE DE CALIBRACIÓN Y ARRANQUE ---
    start_time = 0
    end_time = 0
    
    while True:
        comando = input("Ingrese comando: ")
        if comando == "s": 
            ser.write(b'S') 
            print(">>> ADQUISICIÓN DE DATOS INICIADA... <<<")
            start_time = time.time() # Inicio del cronómetro
            break
        elif comando in ['a', 'd']:
            ser.write(comando.encode()) 
        else:
            print("Comando no reconocido.")

    # --- BUCLE DE ADQUISICIÓN DE DATOS ---
    while True:
        try:
            linea = ser.readline().decode('utf-8', errors='ignore').strip()
        except:
            continue
            
        if linea == "FIN":
            end_time = time.time() # Fin del cronómetro
            print("\nProceso finalizado por el controlador.")
            break
            
        if "," in linea:
            try:
                partes = linea.split(',')
                if len(partes) == 3:
                    deg = float(partes[0])
                    rad = float(partes[1])
                    mag = float(partes[2])
                    
                    # Almacenamiento de datos
                    angulos.append(np.radians(deg))
                    radios.append(rad)
                    campos.append(mag)
                    
                    # Monitoreo en tiempo real
                    tiempo_transcurrido = time.time() - start_time
                    print(f"T: {tiempo_transcurrido:.1f}s | R: {rad}mm | Ang: {deg}° | B: {mag:.2f} mT")
            except ValueError:
                pass

except serial.SerialException:
     print("Error crítico: No se detectó el dispositivo en el puerto especificado.")

ser.close()

# --- PROCESAMIENTO Y VISUALIZACIÓN ---
if len(angulos) > 0:
    # Cálculo del tiempo total
    duracion_total = end_time - start_time
    minutos = int(duracion_total // 60)
    segundos = int(duracion_total % 60)
    tiempo_texto = f"Tiempo de Escaneo: {minutos}m {segundos}s"
    print(f"\nGenerando reporte gráfico. Duración total: {minutos}m {segundos}s")

    # Conversión de Coordenadas Polares a Cartesianas
    x = np.array(radios) * np.cos(np.array(angulos))
    y = np.array(radios) * np.sin(np.array(angulos))
    z = np.array(campos)

    fig, ax = plt.subplots(figsize=(10, 8))
    
    # Generación del mapa de contornos (Interpolación)
    contour = ax.tricontourf(x, y, z, levels=100, cmap="inferno") 
    cbar = plt.colorbar(contour)
    cbar.set_label("Intensidad de Campo Magnético (mT)", rotation=270, labelpad=15)
    
    # Configuración de etiquetas y títulos
    ax.set_title(f"Mapa de Campo Magnético\n{tiempo_texto}")
    ax.set_xlabel("Distancia X (mm)")
    ax.set_ylabel("Distancia Y (mm)")
    
    # Referencia visual del elemento bajo prueba
    circulo_iman = plt.Circle((0, 0), 30, color='white', fill=False, linestyle='--', linewidth=1, label='Perímetro Imán')
    ax.add_artist(circulo_iman)
    
    ax.set_aspect('equal')
    plt.legend(loc='upper right')

    #Extra: Guardar datos en un archivo CSV para posterior análisis
    with open('datos_campo_magnetico.csv', 'w', newline='') as file:
        writer = csv.writer(file)
        writer.writerow(["Angulo_Rad", "Radio_mm", "Campo_mT"])
        for i in range(len(angulos)):
            writer.writerow([angulos[i], radios[i], campos[i]])
    print("Datos exportados a 'datos_campo_magnetico.csv'")

    plt.show()
else:
    print("Advertencia: El conjunto de datos está vacío.")