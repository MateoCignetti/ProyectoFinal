import os
import re

def update_squareline_project(directory, prefix):
    """
    Renombra archivos de SquareLine Studio con un prefijo y actualiza
    su contenido (include guards y sentencias #include) automáticamente.

    Args:
        directory (str): La ruta a la carpeta 'ui' que contiene los archivos.
        prefix (str): El prefijo a agregar (ej. "mi_app_").
    """
    if not os.path.isdir(directory):
        print(f"❌ Error: La carpeta '{directory}' no se encontró.")
        return

    # --- Paso 1: Renombrar archivos y crear un mapa de cambios ---
    # Guardaremos los nombres antiguos y nuevos para saber qué reemplazar después.
    # Ejemplo: rename_map['ui.h'] = 'mi_app_ui.h'
    rename_map = {}
    files_to_process = [f for f in os.listdir(directory) if f.endswith(('.c', '.h'))]

    print("--- 📂 Paso 1: Renombrando archivos ---")
    for filename in files_to_process:
        new_filename = f"{prefix}{filename}"
        old_filepath = os.path.join(directory, filename)
        new_filepath = os.path.join(directory, new_filename)

        try:
            os.rename(old_filepath, new_filepath)
            rename_map[filename] = new_filename
            print(f"✅ '{filename}'  ➔  '{new_filename}'")
        except OSError as e:
            print(f"❌ Error al renombrar '{filename}': {e}")

    if not rename_map:
        print("🤷 No se encontraron archivos .c o .h para procesar.")
        return

    # --- Paso 2: Actualizar el contenido de los archivos renombrados ---
    print("\n--- 📝 Paso 2: Actualizando contenido interno ---")
    for old_name, new_name in rename_map.items():
        filepath = os.path.join(directory, new_name)

        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()

            original_content = content

            # A. Si es un archivo .h, actualiza sus include guards
            if new_name.endswith('.h'):
                # Crea el nuevo formato del guard a partir del nuevo nombre de archivo
                # Ejemplo: "mi_app_ui_helpers.h" -> "MI_APP_UI_HELPERS_H"
                base_name = os.path.splitext(new_name)[0]
                new_guard = base_name.upper() + '_H'

                # Usa una expresión regular para reemplazar los #ifndef y #define antiguos
                content = re.sub(r'(#ifndef\s+)[A-Za-z0-9_]+_H', rf'\1{new_guard}', content)
                content = re.sub(r'(#define\s+)[A-Za-z0-9_]+_H', rf'\1{new_guard}', content)

            # B. Actualiza todas las sentencias #include
            for old_header, new_header in rename_map.items():
                if old_header.endswith('.h'): # Solo nos interesan los includes de headers
                    old_include = f'#include "{old_header}"'
                    new_include = f'#include "{new_header}"'
                    content = content.replace(old_include, new_include)

            # Solo reescribe el archivo si hubo cambios
            if content != original_content:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)
                print(f"🔧 Contenido actualizado en '{new_name}'")

        except Exception as e:
            print(f"❌ Error procesando el contenido de '{new_name}': {e}")

    print("\n¡Proceso completado! 🎉 Tu proyecto está listo.")

# --- Bloque principal de ejecución ---
if __name__ == "__main__":
    target_folder = input("Ingresa la ruta a la carpeta 'ui' exportada: ")
    project_prefix = input("Ingresa el prefijo que quieres usar (ej. mi_app_): ")

    update_squareline_project(target_folder, project_prefix)
