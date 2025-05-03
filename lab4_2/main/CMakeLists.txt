set(srcs "lab4_2.c"
         "esp_hid_gap.c")
set(include_dirs "."
                 "../../esp-idf/components/bt/host/nimble/nimble/nimble/host/include"
                 "../../esp-idf/components/bt/host/nimble/nimble/nimble/include"
                 "../../esp-idf/components/bt/host/nimble/nimble/porting/nimble/include"
                 "../../esp-idf/components/bt/host/nimble/port/include"
                 "../../esp-idf/components/bt/host/nimble/nimble/nimble/include")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "${include_dirs}"
                       REQUIRES esp_hid
                       PRIV_REQUIRES nvs_flash)
