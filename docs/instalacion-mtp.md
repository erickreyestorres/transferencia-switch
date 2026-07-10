# Instalación mediante MTP

## Flujo 0.5.4

`5: SD Card install` es una vista funcional, no un directorio. Cuando Windows envía
un objeto, el servidor selecciona `IncomingObjectSink` por Storage ID y entrega el
flujo secuencial al adaptador `SdInstallReceiverFactory`.

1. Se aceptan `.nsp` y `.xci` sin compresión.
2. Para NSP, la cabecera PFS0 se mantiene en memoria y se validan cantidades, offsets, nombres,
   límites y tamaño total.
3. Para XCI se valida HFS0 raíz, se localiza `secure` y se valida su HFS0 interno.
4. Cada NCA se transmite directamente a un placeholder del `ContentStorage` SD. En
   XCI, su cabecera se convierte de distribución Gamecard a instalable mediante AES-XTS.
5. Si una NCA ya existe en `ContentStorage` y su tamaño coincide, se reutiliza y se
   informa como contenido ya instalado en lugar de tratarlo como fallo.
6. Al completar una NCA nueva se sincroniza y registra mediante NCM.
7. El CNMT instalado se abre con `FsFileSystemType_ContentMeta`.
8. Se comprueba que todas las NCAs declaradas existan y coincidan en tamaño.
9. Si existen ticket y certificado emparejados, se importan mediante ES.
10. Se confirma `ContentMetaDatabase` y se publica `ApplicationRecord`.
11. Ante cancelación o error se eliminan el placeholder activo, los metadatos y las
   NCAs nuevas registradas por esa operación.
12. El servidor responde el resultado final MTP a Windows y solo después destruye el
   receptor y cierra sus sesiones NCM/NS/ES.
13. El ObjectHandle virtual permanece disponible para las consultas finales de Windows
   y se elimina al recibir `SessionEnded`; el paquete no se conserva en la SD.
14. Si el adaptador rechaza un bloque, el servidor drena los bytes restantes para no
   desalinear MTP y presenta el motivo original de la falla.
15. Una colección puede contener hasta 32 CNMT; cada registro se confirma y el rollback
   elimina todos los ContentMeta y contenidos nuevos de la operación.

El paquete completo no se guarda en FAT32, por lo que no se duplica el espacio. La
recepción de objetos con tamaño MTP desconocido (`0xFFFFFFFF`) está en fase
experimental y se valida con fixtures/simuladores antes de pasar a hardware.

## Formatos posteriores

- NSZ/XCZ: descompresión NCZ por streaming con verificación del tamaño expandido.
- `6: NAND install`: el mismo caso de uso con `NcmStorageId_BuiltInUser`, habilitado
  solamente después de validar la ruta SD.
