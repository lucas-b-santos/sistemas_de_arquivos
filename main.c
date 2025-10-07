#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Trabalho Leitura Imagem FAT16
// Alunos: Lucas B. Santos

#define FAT_ENTRY_SIZE 2

typedef struct fat_BS
{
	unsigned char bootjmp[3];
	unsigned char oem_name[8];
	unsigned short bytes_per_sector;
	unsigned char sectors_per_cluster;
	unsigned short reserved_sector_count;
	unsigned char table_count;
	unsigned short root_entry_count;
	unsigned short total_sectors_16;
	unsigned char media_type;
	unsigned short table_size_16;
	unsigned short sectors_per_track;
	unsigned short head_side_count;
	unsigned int hidden_sector_count;
	unsigned int total_sectors_32;

	// this will be cast to it's specific type once the driver actually knows what type of FAT this is.
	unsigned char extended_section[54];

} __attribute__((packed)) fat_BS_t;

void read_file(FILE *fp, fat_BS_t boot_record, unsigned short first_cluster, int cluster_number, int file_size)
{
	int bytes_per_sector = boot_record.bytes_per_sector;
	int first_fat_sector = boot_record.reserved_sector_count;
	int sectors_per_cluster = boot_record.sectors_per_cluster;
	int root_dir_sectors = (boot_record.root_entry_count * 32 + (bytes_per_sector - 1)) / bytes_per_sector;
	int first_data_sector = boot_record.reserved_sector_count + boot_record.table_count * boot_record.table_size_16 + root_dir_sectors;
	int bytes_per_cluster = sectors_per_cluster * bytes_per_sector;

	unsigned short entrada;
	unsigned short clusters[cluster_number];

	clusters[0] = first_cluster;
	entrada = first_cluster;

	for (int i = 1; i < cluster_number; i++)
	{
		int offset = first_fat_sector * bytes_per_sector + entrada * FAT_ENTRY_SIZE;
		fseek(fp, offset, SEEK_SET);
		fread(&entrada, FAT_ENTRY_SIZE, 1, fp);
		clusters[i] = entrada;
	}

	for (int i = 0; i < cluster_number; i++)
	{
		int offset = (first_data_sector + (clusters[i] - 2) * sectors_per_cluster) * bytes_per_sector;
		fseek(fp, offset, SEEK_SET);
		printf("\nConteudo do Cluster %i: \n", clusters[i]);
		int size =  (i == cluster_number - 1) ? file_size % bytes_per_cluster : bytes_per_cluster;
		for (int j = 0; j < size; j++)
		{
			unsigned char byte_lido;
			fread(&byte_lido, 1, 1, fp);
			printf("%c", byte_lido);
			if ((j + 1) % 100 == 0)
			{
				printf("\n");
			}
		}
	}
}

void read_directory(FILE *fp, fat_BS_t boot_record, char *dir_name, unsigned short first_cluster, int read_content)
{
	int cluster_number = 1;
	int bytes_per_sector = boot_record.bytes_per_sector;
	int first_fat_sector = boot_record.reserved_sector_count;
	int sectors_per_cluster = boot_record.sectors_per_cluster;
	int root_dir_sectors = (boot_record.root_entry_count * 32 + (boot_record.bytes_per_sector - 1)) / boot_record.bytes_per_sector;
	int first_data_sector = boot_record.reserved_sector_count + boot_record.table_count * boot_record.table_size_16 + root_dir_sectors;
	int first_root_dir_sector = first_data_sector - root_dir_sectors;

	unsigned short entrada;

	entrada = first_cluster;

	while (1)
	{
		int offset = first_fat_sector * bytes_per_sector + entrada * FAT_ENTRY_SIZE;
		fseek(fp, offset, SEEK_SET);
		fread(&entrada, FAT_ENTRY_SIZE, 1, fp);

		if (entrada >= 0xFFF8 && entrada <= 0xFFFF)
		{
			break; // End of cluster chain
		}
		
		cluster_number++;
	}

	unsigned short clusters[cluster_number];

	int bytes_per_cluster = sectors_per_cluster * bytes_per_sector;

	clusters[0] = first_cluster;
	printf("\n\n=======ENTRADAS DIRETORIO %s=======\n\n", dir_name);

	for (int n_cluster = 0; n_cluster < cluster_number; n_cluster++)
	{
		// desloca para o cluster na area de dados
		int offset = (first_data_sector + (clusters[n_cluster] - 2) * sectors_per_cluster) * bytes_per_sector;

		for (int index = 0; index < bytes_per_cluster; index += 32)
		{
			unsigned char byte_lido;
			fseek(fp, offset + index, SEEK_SET);
			fread(&byte_lido, 1, 1, fp);

			if (byte_lido == 0x00)
			{
				break; // No more entries
			}

			if (byte_lido == 0xE5)
			{
				continue; // Deleted entry
			}

			fseek(fp, offset + index + 11, SEEK_SET);
			fread(&byte_lido, 1, 1, fp);

			// 0x20 = Arquivo, 0x10 = Diretório

			if (byte_lido == 0x20)
			{
				printf("Nome do arquivo: ");
				for (int i = 0; i < 11; i++)
				{
					char c;
					fseek(fp, offset + index + i, SEEK_SET);
					fread(&c, 1, 1, fp);
					printf("%c", c);
				}
				printf("\n");
				printf("Primeiro Cluster do Arquivo: ");

				unsigned short file_cluster;
				fseek(fp, offset + index + 26, SEEK_SET);
				fread(&file_cluster, 2, 1, fp);
				printf("%i", file_cluster);

				printf("\n");
				printf("Tamanho do arquivo em bytes: ");

				int file_size;
				fseek(fp, offset + index + 28, SEEK_SET);
				fread(&file_size, 4, 1, fp);
				printf("%i", file_size);

				int cluster_number = (file_size + bytes_per_cluster - 1) / bytes_per_cluster;

				if (read_content)
				{
					read_file(fp, boot_record, file_cluster, cluster_number, file_size);
				}

				printf("\n=====================================\n");
			}

			else if (byte_lido == 0x10)
			{
				printf("Nome do diretório: ");
				char current_dir_name[12];

				for (int i = 0; i < 11; i++)
				{
					char c;
					fseek(fp, offset + index + i, SEEK_SET);
					fread(&c, 1, 1, fp);
					printf("%c", c);
					current_dir_name[i] = c;
				}
				printf("\n");
				printf("Primeiro Cluster do Diretório: ");

				short dir_cluster;
				fseek(fp, offset + index + 26, SEEK_SET);
				fread(&dir_cluster, 2, 1, fp);
				printf("%i", dir_cluster);

				if (current_dir_name[0] != '.')
					read_directory(fp, boot_record, current_dir_name, dir_cluster, read_content);

				printf("\n=====================================\n");
			}
		}
	}

	printf("\n=======FIM DAS ENTRADAS DIRETORIO %s=======\n", dir_name);
}

int main(int argc, char *argv[])
{
	int read_content = 0;
	if (argc < 2)
	{
		printf("Uso: %s <fat16_img_file> [-r] \n", argv[0]);
		printf("-r: ler conteudo dos arquivos\n");
		return 1;
	}
	else if (argc == 3 && strcmp(argv[2], "-r") == 0)
	{
		read_content = 1;
	}

	FILE *fp;
	fat_BS_t boot_record;

	fp = fopen(argv[1], "rb");

	if (fp == NULL)
	{
		perror("Erro ao abrir o arquivo");
		return 1;
	}

	fseek(fp, 0, SEEK_SET);
	fread(&boot_record, sizeof(fat_BS_t), 1, fp);

	printf("Bytes por setor: %hd \n", boot_record.bytes_per_sector);
	printf("Setores por cluster: %x \n", boot_record.sectors_per_cluster);
	printf("Numero Tabelas FAT: %x \n", boot_record.table_count);
	printf("Tamanho tabela FAT: %i \n", boot_record.table_size_16);
	printf("Num Setores Reservados: %x \n", boot_record.reserved_sector_count);
	printf("Num Entradas Diretorio Raiz: %i \n", boot_record.root_entry_count);

	int fat_size = (boot_record.table_size_16 == 0) ? boot_record.table_size_16 : boot_record.table_size_16;

	for (int i = 0; i < boot_record.table_count; i++)
	{
		printf("Setor Inicial Tabela FAT %i: %i \n", i + 1, boot_record.reserved_sector_count + i * fat_size);
	}

	int root_dir_sectors = (boot_record.root_entry_count * 32 + (boot_record.bytes_per_sector - 1)) / boot_record.bytes_per_sector;

	int first_data_sector = boot_record.reserved_sector_count + boot_record.table_count * boot_record.table_size_16 + root_dir_sectors;

	printf("Setor Inicial Area de Dados: %i \n", first_data_sector);

	int first_root_dir_sector = first_data_sector - root_dir_sectors;

	printf("Setor Inicial Diretorio Raiz: %i \n", first_root_dir_sector);

	unsigned char byte_lido;
	int index = 0;

	printf("\n=======ENTRADAS DIRETORIO RAIZ=======\n");

	for (int i = 0; i < boot_record.root_entry_count; i++)
	{
		int offset = first_root_dir_sector * boot_record.bytes_per_sector + index;

		fseek(fp, offset, SEEK_SET);
		fread(&byte_lido, 1, 1, fp);

		if (byte_lido == 0x00)
		{
			break; // No more entries
		}

		if (byte_lido == 0xE5)
		{
			index += 32;
			continue; // Deleted entry
		}

		fseek(fp, offset + 11, SEEK_SET);
		fread(&byte_lido, 1, 1, fp);

		// 0x20 = Arquivo, 0x10 = Diretório

		if (byte_lido == 0x20)
		{
			printf("Nome do arquivo: ");
			for (int i = 0; i < 11; i++)
			{
				char c;
				fseek(fp, first_root_dir_sector * boot_record.bytes_per_sector + index + i, SEEK_SET);
				fread(&c, 1, 1, fp);
				printf("%c", c);
			}
			printf("\n");
			printf("Primeiro Cluster do Arquivo: ");

			unsigned short file_cluster;
			fseek(fp, first_root_dir_sector * boot_record.bytes_per_sector + index + 26, SEEK_SET);
			fread(&file_cluster, 2, 1, fp);
			printf("%i", file_cluster);

			printf("\n");
			printf("Tamanho do arquivo em bytes: ");

			int file_size;
			fseek(fp, first_root_dir_sector * boot_record.bytes_per_sector + index + 28, SEEK_SET);
			fread(&file_size, 4, 1, fp);
			printf("%i", file_size);

			int bytes_per_cluster = boot_record.sectors_per_cluster * boot_record.bytes_per_sector;

			int cluster_number = (file_size + bytes_per_cluster - 1) / bytes_per_cluster;

			if (read_content)
			{
				read_file(fp, boot_record, file_cluster, cluster_number, file_size);
			}

			printf("\n=====================================\n");
		}

		else if (byte_lido == 0x10)
		{
			printf("Nome do diretório: ");
			char dir_name[12];
			for (int i = 0; i < 11; i++)
			{
				char c;
				fseek(fp, first_root_dir_sector * boot_record.bytes_per_sector + index + i, SEEK_SET);
				fread(&c, 1, 1, fp);
				printf("%c", c);
				dir_name[i] = c;
			}
			printf("\n");
			printf("Primeiro Cluster do Diretório: ");

			unsigned short dir_cluster;
			fseek(fp, first_root_dir_sector * boot_record.bytes_per_sector + index + 26, SEEK_SET);
			fread(&dir_cluster, 2, 1, fp);
			printf("%i", dir_cluster);
			read_directory(fp, boot_record, dir_name, dir_cluster, read_content);
			printf("\n=====================================\n");
		}

		index += 32;
	}

	printf("\n=======FIM DAS ENTRADAS DIRETORIO RAIZ=======\n");

	return 0;
}
