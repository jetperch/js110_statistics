/*
 * Copyright 2020 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _USB_DEF_H_
#define _USB_DEF_H_


/* USB standard request type */
#define USB_REQUEST_TYPE_DIR_MASK (0x80U)
#define USB_REQUEST_TYPE_DIR_SHIFT (7U)
#define USB_REQUEST_TYPE_DIR_OUT (0U)
#define USB_REQUEST_TYPE_DIR_IN  (1U)

#define USB_REQUEST_TYPE_TYPE_MASK (0x60U)
#define USB_REQUEST_TYPE_TYPE_SHIFT (5U)
#define USB_REQUEST_TYPE_TYPE_STANDARD  (0U)
#define USB_REQUEST_TYPE_TYPE_CLASS     (1U)
#define USB_REQUEST_TYPE_TYPE_VENDOR    (2U)
#define USB_REQUEST_TYPE_TYPE_RESERVED  (3U)

#define USB_REQUEST_TYPE_RECIPIENT_MASK (0x1FU)
#define USB_REQUEST_TYPE_RECIPIENT_SHIFT (0U)
#define USB_REQUEST_TYPE_RECIPIENT_DEVICE       (0U)
#define USB_REQUEST_TYPE_RECIPIENT_INTERFACE    (1U)
#define USB_REQUEST_TYPE_RECIPIENT_ENDPOINT     (2U)
#define USB_REQUEST_TYPE_RECIPIENT_OTHER        (3U)

#define USB_REQUEST_TYPE(recipient, type, direction) \
    ((USB_REQUEST_TYPE_RECIPIENT_##recipient << USB_REQUEST_TYPE_RECIPIENT_SHIFT) | \
     (USB_REQUEST_TYPE_TYPE_##type << USB_REQUEST_TYPE_TYPE_SHIFT) | \
     (USB_REQUEST_TYPE_DIR_##direction << USB_REQUEST_TYPE_DIR_SHIFT))


// JS110-specific USB definitions
#define JS110_USBREQ_SETTINGS               (3)
#define JS110_USBREQ_STATUS                 (4)


#endif /* _USB_DEF_H_ */
